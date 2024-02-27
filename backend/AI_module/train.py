import json
import time

import torch.optim
import torch.utils.data
import torchvision.transforms as transforms
from nltk.translate.bleu_score import corpus_bleu
from torch import nn
from torch.nn.utils.rnn import pack_padded_sequence

from config import *
from data_generator import CaptionDataset
from models import Encoder, DecoderWithAttention
from utils import *

from torch.utils.tensorboard import SummaryWriter

log_writer = SummaryWriter()

def main():
    """
    train and validate.
    """

    global best_bleu4, epochs_since_improvement, checkpoint, start_epoch, fine_tune_encoder, word_map

    # read word map
    word_map_file = os.path.join(data_folder, 'WORDMAP.json')
    with open(word_map_file, 'r') as j:
        word_map = json.load(j)

    # initialize or load checkpoint
    if checkpoint is None:
        decoder = DecoderWithAttention(attention_dim = attention_dim,
                                       embed_dim = emb_dim,
                                       decoder_dim = decoder_dim,
                                       vocab_size = len(word_map),
                                       dropout = dropout)
        decoder_optimizer = torch.optim.Adam(params = filter(lambda p: p.requires_grad, decoder.parameters()),
                                             lr = decoder_lr)
        
        encoder = Encoder()
        encoder.fine_tune(fine_tune_encoder)
        encoder_optimizer = torch.optim.Adam(params = filter(lambda p: p.requires_grad, encoder.parameters()),
                                             lr = encoder_lr) if fine_tune_encoder else None
    else:
        checkpoint = torch.load(checkpoint)
        start_epoch = checkpoint['epoch'] + 1
        epochs_since_improvement = checkpoint['epochs_since_improvement']
        best_bleu4 = checkpoint['bleu-4']

        decoder = checkpoint['decoder']
        decoder_optimizer = checkpoint['decoder_optimizer']
        encoder = checkpoint['encoder']
        encoder_optimizer = checkpoint['encoder_optimizer']

        if fine_tune_encoder is True and encoder_optimizer is None:
            encoder.fine_tune(fine_tune_encoder)
            encoder_optimizer = torch.optim.Adam(params = filter(lambda p: p.requires_grad, encoder.parameters()),
                                                 lr = encoder_lr)

    # move to GPU
    decoder = decoder.to(device)
    encoder = encoder.to(device)

    # loss function
    criterion = nn.CrossEntropyLoss().to(device)

    # custom dataloaders
    normalize = transforms.Normalize(mean = [0.485, 0.456, 0.406],
                                     std = [0.229, 0.224, 0.225])
    train_loader = torch.utils.data.DataLoader(CaptionDataset('train', transform = transforms.Compose([normalize])),
                                               batch_size = batch_size, 
                                               shuffle = True, 
                                               num_workers = workers, 
                                               pin_memory = True)
    val_loader = torch.utils.data.DataLoader(CaptionDataset('valid', transform = transforms.Compose([normalize])),
                                             batch_size = batch_size, 
                                             shuffle = False, 
                                             num_workers = workers, 
                                             pin_memory = True)

    # epochs
    for epoch in range(start_epoch, epochs):
        # decay learning rate if there is no improvement for 8 consecutive epochs, 
        # and terminate training after 20
        if epochs_since_improvement == 20:
            break
        if epochs_since_improvement > 0 and epochs_since_improvement % 8 == 0:
            adjust_learning_rate(decoder_optimizer, 0.8)
            if fine_tune_encoder:
                adjust_learning_rate(encoder_optimizer, 0.8)

        # one epoch's training
        train(train_loader = train_loader,
              encoder = encoder,
              decoder = decoder,
              criterion = criterion,
              encoder_optimizer = encoder_optimizer,
              decoder_optimizer = decoder_optimizer,
              epoch = epoch)

        # one epoch's validation
        recent_bleu4 = validate(val_loader = val_loader,
                                encoder = encoder,
                                decoder = decoder,
                                criterion = criterion)

        # check if there was an improvement
        is_best = recent_bleu4 > best_bleu4
        best_bleu4 = max(recent_bleu4, best_bleu4)
        if not is_best:
            epochs_since_improvement += 1
            print("[*] epochs since last improvement: {}".format(epochs_since_improvement))
        else:
            epochs_since_improvement = 0

        # save checkpoint
        save_checkpoint(epoch, epochs_since_improvement, 
                        encoder, decoder, encoder_optimizer,
                        decoder_optimizer, recent_bleu4, is_best)

def train(train_loader, encoder, decoder, criterion, encoder_optimizer, decoder_optimizer, epoch):
    """
    performs one epoch's training
    :param train_loader: DataLoader for training data
    :param encoder: encoder model
    :param decoder: decoder model
    :param criterion: loss layer
    :param encoder_optimizer: optimizer to update encoder's weights (if fine-tuning)
    :param decoder_optimizer: optimizer to update decoder's weights
    :param epoch: epoch number
    """
    
    # train mode (dropout and batchnorm is used)
    decoder.train()
    encoder.train()

    batch_time = AverageMeter()       # forward prop. + back prop. time
    data_time = AverageMeter()        # data loading time
    losses = AverageMeter()           # loss (per word decoded)
    top5accs = AverageMeter()         # top5 accuracy

    start = time.time()

    # batches
    for i, (imgs, caps, caplens) in enumerate(train_loader):
        data_time.update(time.time() - start)

        # move to GPU
        imgs = imgs.to(device)
        caps = caps.to(device)
        caplens = caplens.to(device)

        # forward prop.
        imgs = encoder(imgs)
        scores, caps_sorted, decode_lengths, alphas, _ = decoder(imgs, caps, caplens)

        # since we decoded starting with <start>, the targets are all words after <start>, up to <end>
        targets = caps_sorted[:, 1:]

        # remove timesteps that we didn't decode at, or are pads
        # pack_padded_sequence is an easy trick to do this
        scores = pack_padded_sequence(scores, decode_lengths, batch_first = True).data
        targets = pack_padded_sequence(targets, decode_lengths, batch_first = True).data

        # calculate loss
        loss = criterion(scores, targets)

        # add doubly stochastic attention regularization
        loss += alpha_c * ((1. - alphas.sum(dim=1)) ** 2).mean()

        # back prop.
        decoder_optimizer.zero_grad()
        if encoder_optimizer is not None:
            encoder_optimizer.zero_grad()
        loss.backward()

        # clip gradients
        if grad_clip is not None:
            clip_gradient(decoder_optimizer, grad_clip)
            if encoder_optimizer is not None:
                clip_gradient(encoder_optimizer, grad_clip)

        # update weights
        decoder_optimizer.step()
        if encoder_optimizer is not None:
            encoder_optimizer.step()

        # keep track of metrics
        top5 = accuracy(scores, targets, 5)
        
        losses.update(loss.item(), sum(decode_lengths))
        top5accs.update(top5, sum(decode_lengths))
        batch_time.update(time.time() - start)

        start = time.time()

        if i % print_freq == 0:
            print("-"*150)
            print('Epoch: [{0}][{1}/{2}]\t'
                  'Batch Time {batch_time.val:.3f} ({batch_time.avg:.3f})\t'
                  'Data Load Time {data_time.val:.3f} ({data_time.avg:.3f})\t'
                  'Loss {loss.val:.4f} ({loss.avg:.4f})\t'
                  'Top-5 Accuracy {top5.val:.3f}% ({top5.avg:.3f}%)'.format(epoch, i, len(train_loader),
                                                                            batch_time = batch_time,
                                                                            data_time = data_time, 
                                                                            loss = losses,
                                                                            top5 = top5accs))
            print("-"*150)
        
    log_writer.add_scalar('Loss/train', float(losses.avg), epoch)
    log_writer.add_scalar('Top5accs/train', float(top5accs.avg), epoch)

def validate(val_loader, encoder, decoder, criterion):
    """
    performs one epoch's validation
    :param val_loader: dataLoader for validation data
    :param encoder: encoder model
    :param decoder: decoder model
    :param criterion: loss layer
    :return: BLEU-4 score
    """

    # eval mode (no dropout or batchnorm)
    decoder.eval() 
    if encoder is not None:
        encoder.eval()

    batch_time = AverageMeter()
    losses = AverageMeter()
    top5accs = AverageMeter()

    start = time.time()

    references = list()  # references (true captions) for calculating BLEU-4 score
    hypotheses = list()  # hypotheses (predictions)

    # batches
    for i, (imgs, caps, caplens, allcaps) in enumerate(val_loader):
        imgs = imgs.to(device)
        caps = caps.to(device)
        caplens = caplens.to(device)
        allcaps = allcaps.to(device)

        # forward prop.
        if encoder is not None:
            imgs = encoder(imgs)
        scores, caps_sorted, decode_lengths, alphas, sort_ind = decoder(imgs, caps, caplens)

        targets = caps_sorted[:, 1:]

        # remove timesteps that we didn't decode at, or are pads
        # pack_padded_sequence is an easy trick to do this
        scores_copy = scores.clone()
        scores = pack_padded_sequence(scores, decode_lengths, batch_first = True).data
        targets  = pack_padded_sequence(targets, decode_lengths, batch_first = True).data

        # calculate loss
        loss = criterion(scores, targets)

        # add doubly stochastic attention regularization
        loss += alpha_c * ((1. - alphas.sum(dim=1)) ** 2).mean()

        # keep track of metrics
        losses.update(loss.item(), sum(decode_lengths))
        top5 = accuracy(scores, targets, 5)
        top5accs.update(top5, sum(decode_lengths))
        batch_time.update(time.time() - start)

        start = time.time()

        if i % print_freq == 0:
            print("-"*150)
            print('Validation: [{0}/{1}]\t'
                  'Batch Time {batch_time.val:.3f} ({batch_time.avg:.3f})\t'
                  'Loss {loss.val:.4f} ({loss.avg:.4f})\t'
                  'Top-5 Accuracy {top5.val:.3f}% ({top5.avg:.3f}%)\t'.format(i, len(val_loader), 
                                                                              batch_time = batch_time,
                                                                              loss = losses, 
                                                                              top5 = top5accs))
            print("-"*150)

        # references
        # because images were sorted in the decoder
        allcaps = allcaps[sort_ind]
        for j in range(allcaps.shape[0]):
            img_caps = allcaps[j].tolist()
            # remove <start> and pads
            img_captions = list(map(lambda c: [w for w in c if w not in {word_map['<start>'], word_map['<pad>']}],
                                    img_caps))  
            references.append(img_captions)

        # hypotheses
        _, preds = torch.max(scores_copy, dim = 2)
        preds = preds.tolist()
        temp_preds = list()
        for j, p in enumerate(preds):
            # remove pads
            temp_preds.append(preds[j][:decode_lengths[j]]) 
        preds = temp_preds
        hypotheses.extend(preds)

        assert len(references) == len(hypotheses)

    # calculate BLEU-4 scores
    bleu4 = corpus_bleu(references, hypotheses)

    print("-"*150)
    print('LOSS - {loss.avg:.3f}, TOP-5 ACCURACY - {top5.avg:.3f}, BLEU-4 - {bleu}'.format(loss = losses,
                                                                                             top5 = top5accs,
                                                                                             bleu = bleu4))
    print("-"*150)

    return bleu4

if __name__ == '__main__':
    main()
