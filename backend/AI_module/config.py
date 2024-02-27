import os
import torch
import torch.backends.cudnn as cudnn

device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
print("device: ", device)

## ---------------------------------------------------------------------------------------------------------
## image parameters

image_h = 256
image_w = 256
image_size = 256
channel = 3
patience = 10
num_train_samples = 10500
num_valid_samples = 1500
max_len = 40
captions_per_image = 5

## ---------------------------------------------------------------------------------------------------------
## model parameters

emb_dim = 512                   # dimension of word embeddings
attention_dim = 512             # dimension of attention linear layers
decoder_dim = 512               # dimension of decoder RNN
dropout = 0.5
cudnn.benchmark = True          # set to true only if inputs to model are fixed size
dataset_number_clip = 0.2       # decrease dataset image number to train and validate faster

## ---------------------------------------------------------------------------------------------------------
## training parameters

start_epoch = 0
epochs = 120                    
epochs_since_improvement = 0    # keeps track of number of epochs since there's been an improvement in validation BLEU
batch_size = 128
workers = 2                     # for data-loading; right now, only 1 works with h5py
encoder_lr = 1e-4               # learning rate for encoder if fine-tuning
decoder_lr = 4e-4               # learning rate for decoder
grad_clip = 5.                  # clip gradients at an absolute value of
alpha_c = 1.                    # regularization parameter for 'doubly stochastic attention'
best_bleu4 = 0.                 # BLEU-4 score right now
print_freq = 10                 # print training/validation stats every x batches
fine_tune_encoder = False       # fine-tune encoder?
checkpoint = "checkpoint_.pth.tar"              # path to checkpoint, None if none
min_word_freq = 3

## ---------------------------------------------------------------------------------------------------------
## path arguments

data_folder = 'data'
train_folder = 'data/ai_challenger_caption_train_20170902'
valid_folder = 'data/ai_challenger_caption_validation_20170910'
test_a_folder = 'data/ai_challenger_caption_test_a_20180103'
test_b_folder = 'data/ai_challenger_caption_test_b_20180103'
train_image_folder = os.path.join(train_folder, 'caption_train_images_20170902')
valid_image_folder = os.path.join(valid_folder, 'caption_validation_images_20170910')
test_a_image_folder = os.path.join(test_a_folder, 'caption_test_a_images_20180103')
test_b_image_folder = os.path.join(test_b_folder, 'caption_test_b_images_20180103')
train_annotations_filename = os.path.join(train_folder, 'caption_train_annotations_20170902.json')
valid_annotations_filename = os.path.join(valid_folder, 'caption_validation_annotations_20170910.json')
test_a_annotations_filename = os.path.join(test_a_folder, 'caption_test_a_annotations_20180103.json')
test_b_annotations_filename = os.path.join(test_b_folder, 'caption_test_b_annotations_20180103.json')
