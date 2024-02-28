# Image caption model data process and train

## Dataset download

Download dataset from [link](https://pan.baidu.com/s/1-idmL-MRv2CuJ-43rRh44w?pwd=iy51). And put it into AI_module/data folder.

## Install dependencies

```
pip install Pillow
pip install numpy
pip install scikit-image
pip3 install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu118
pip install matplotlib
```

## Data process

```
python pre_process.py
```

## Train

It cost a lot of time (few days). You can download my trained model from [link2](https://pan.baidu.com/s/1Z_HpNYelnAezDv0xUHiWvA?pwd=rvp6).

```
python train.py
```

If you want to use tensorboard to show train process, you can use:

```
tensorboard --logdir=runs
```

## Run demo

```
python demo.py --img image_path --model BEST_checkpoint_.pth.tar --word_map data/WORDMAP.json --beam_size 5
```

