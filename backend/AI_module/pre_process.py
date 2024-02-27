import json
import jieba
import zipfile
from tqdm import tqdm
from collections import Counter

from config import *
from utils import ensure_folder

def extract(folder):
    filename = "{}.zip".format(folder)
    print("[+] extracting {}...".format(filename))
    with zipfile.ZipFile(filename, 'r') as zip_ref:
        zip_ref.extractall('data')

def create_input_files():
    """
    json data structure:
    [
        {
            "url": "http://m4.biz.itc.cn/pic/new/n/71/65/Img8296571_n.jpg", 
            "image_id": "8f00f3d0f1008e085ab660e70dffced16a8259f6.jpg", 
            "caption": [
                "\u4e24\u4e2a\u8863\u7740\u4f11\u95f2\u7684\u4eba\u5728\u5e73\u6574\u7684\u9053\u8def\u4e0a\u4ea4\u8c08", 
                "\u4e00\u4e2a\u7a7f\u7740\u7ea2\u8272\u4e0a\u8863\u7684\u7537\u4eba\u548c\u4e00\u4e2a\u7a7f\u7740\u7070\u8272\u88e4\u5b50\u7684\u7537\u4eba\u7ad9\u5728\u5ba4\u5916\u7684\u9053\u8def\u4e0a\u4ea4\u8c08", 
                "\u5ba4\u5916\u7684\u516c\u56ed\u91cc\u6709\u4e24\u4e2a\u7a7f\u7740\u957f\u88e4\u7684\u7537\u4eba\u5728\u4ea4\u6d41", 
                "\u8857\u9053\u4e0a\u6709\u4e00\u4e2a\u7a7f\u7740\u6df1\u8272\u5916\u5957\u7684\u7537\u4eba\u548c\u4e00\u4e2a\u7a7f\u7740\u7ea2\u8272\u5916\u5957\u7684\u7537\u4eba\u5728\u4ea4\u8c08", 
                "\u9053\u8def\u4e0a\u6709\u4e00\u4e2a\u8eab\u7a7f\u7ea2\u8272\u4e0a\u8863\u7684\u7537\u4eba\u5728\u548c\u4e00\u4e2a\u62ac\u7740\u5de6\u624b\u7684\u4eba\u8bb2\u8bdd"
            ]
        }, 
        {
            "url": "http://pic-bucket.nosdn.127.net/photo/0426/2017-01-23/CBFSJR2975I30426NOS.jpg", 
            "image_id": "b96ff46ba5b1cbe5bb4cc32b566431132ca71a64.jpg", 
            "caption": [
                "\u623f\u95f4\u91cc\u6709\u4e09\u4e2a\u5750\u5728\u684c\u5b50\u65c1\u7684\u4eba\u5728\u5403\u996d", 
                "\u4e24\u4e2a\u6234\u7740\u5e3d\u5b50\u7684\u4eba\u548c\u4e00\u4e2a\u77ed\u53d1\u7537\u4eba\u5750\u5728\u623f\u95f4\u91cc\u5c31\u9910", 
                "\u623f\u95f4\u91cc\u6709\u4e00\u4e2a\u7537\u4eba\u548c\u4e24\u4e2a\u6234\u5e3d\u5b50\u7684\u8001\u4eba\u5728\u5403\u996d", 
                "\u5ba4\u5185\u4e09\u4e2a\u8863\u7740\u5404\u5f02\u7684\u4eba\u5750\u5728\u684c\u5b50\u65c1\u4ea4\u8c08", 
                "\u5c4b\u5b50\u91cc\u6709\u4e09\u4e2a\u8863\u7740\u5404\u5f02\u7684\u4eba\u5750\u5728\u6905\u5b50\u4e0a"
            ]
        },
        ... 
    ] 
    """
    json_path = train_annotations_filename

    # read caption_train_annotations_20170902.json
    with open(json_path, 'r') as j:
        samples = json.load(j)

    # calculate word frequecy
    word_freq = Counter()
    for sample in tqdm(samples):
        caption = sample['caption']
        for c in caption:
            seg_list = jieba.cut(c, cut_all = True)
            word_freq.update(seg_list)

    # create word map
    """
    word_map = {"<pad>":0, "apple":1, "human":2, "banana":3, ..., "<unk>": 9999, "<start>":9999, "<end>":9999}
    """
    words = [w for w in word_freq.keys() if word_freq[w] > min_word_freq]
    word_map = {k: index + 1 for index, k in enumerate(words)}
    word_map['<unk>'] = len(word_map) + 1
    word_map['<start>'] = len(word_map) + 1
    word_map['<end>'] = len(word_map) + 1
    word_map['<pad>'] = 0

    print("[*] word count: {}".format(len(word_map)))
    print("[*] top 10 words:")
    print(words[:10])

    # save word map to data directory
    with open(os.path.join(data_folder, 'WORDMAP.json'), 'w') as j:
        json.dump(word_map, j)

if __name__ == '__main__':
    # make sure data directory is exist
    ensure_folder('data')

    # extract image to data directory
    if not os.path.isdir(train_image_folder):
        extract(train_folder)
    if not os.path.isdir(valid_image_folder):
        extract(valid_folder)
    if not os.path.isdir(test_a_image_folder):
        extract(test_a_folder)
    if not os.path.isdir(test_b_image_folder):
        extract(test_b_folder)

    # create word map
    create_input_files()
