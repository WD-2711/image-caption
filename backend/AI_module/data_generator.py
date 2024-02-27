import json
import jieba
import numpy as np
from imageio import imread
from skimage.transform import resize as imresize
from torch.utils.data import Dataset

from config import *

def encode_caption(word_map, c):
    return [word_map['<start>']] + \
           [word_map.get(word, word_map['<unk>']) for word in c] + \
           [word_map['<end>']] + \
           [word_map['<pad>']] * (max_len - len(c))

class CaptionDataset(Dataset):
    """
    a pytorch dataset class to be used in a PyTorch DataLoader to create batches
    """
    def __init__(self, split, transform=None):
        """
        :param split: split to 'train' or 'valid'
        :param transform: image transform pipeline
        """
        self.split = split
        assert self.split in {'train', 'valid'}

        if split == 'train':
            json_path = train_annotations_filename
            self.image_folder = train_image_folder
        else:
            json_path = valid_annotations_filename
            self.image_folder = valid_image_folder

        # read JSON
        with open(json_path, 'r') as j:
            self.samples = json.load(j)

        # read wordmap
        with open(os.path.join(data_folder, 'WORDMAP.json'), 'r') as j:
            self.word_map = json.load(j)

        # pytorch transformation pipeline for the image (normalizing, etc.)
        self.transform = transform

        # total number of datapoints
        self.dataset_size = int(len(self.samples * captions_per_image) * dataset_number_clip)

    def __getitem__(self, i):
        # the Nth caption corresponds to the (N // captions_per_image)th image
        sample = self.samples[i // captions_per_image]
        path = os.path.join(self.image_folder, sample['image_id'])
        
        # read images
        img = imread(path)
        if len(img.shape) == 2:
            img = img[:, :, np.newaxis]
            img = np.concatenate([img, img, img], axis=2)
        img = imresize(img, (256, 256))
        img = img.transpose(2, 0, 1)
        assert img.shape == (3, 256, 256)
        assert np.max(img) <= 255
        img = torch.FloatTensor(img / 255.)
        if self.transform is not None:
            img = self.transform(img)

        # sample captions
        captions = sample['caption']

        # sanity check
        assert len(captions) == captions_per_image
        c = captions[i % captions_per_image]
        c = list(jieba.cut(c))

        # encode captions
        enc_c = encode_caption(self.word_map, c)
        caption = torch.LongTensor(enc_c)
        caplen = torch.LongTensor([len(c) + 2])

        if self.split == 'train':
            return img, caption, caplen
        else:
            # for validation of testing, also return all 'captions_per_image' captions to find BLEU-4 score
            all_captions = torch.LongTensor([encode_caption(self.word_map, list(jieba.cut(c))) for c in captions])
            return img, caption, caplen, all_captions

    def __len__(self):
        return self.dataset_size
