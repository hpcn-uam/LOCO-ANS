# LOCO-ANS CODEC 

This repository contains the source code resulting from the development of an ANS based coder for JPEG-LS.

## Publication
This work has an associated publication (currently under revision):

Title: "LOCO-ANS: An optimization of JPEG-LS using anefficient and low complexity coder based on ANS"

Authors: Tobías Alonso, Gustavo Sutter, and Jorge E. López de Vergara

Summited to IEEE Access


## Contents:
- codec: source code of LOCO-ANS codec
- notebooks: 
  - coder_config_gen.ipynb: Jupyter notebook to select coder parameters and generate the configuration file and tANS tables
- results_images: Comparisons between LOCO-ANS and JPEG-LS for different datasets
- Test: test code
- tools: auxiliary tools

## Test

A quick test can be run using the BASH script under the Test folder.
To run the test, the codec and auxiliary tools need to be built (run `make` in the repo root)

Prerequisites:
- [OpenCV C++ lib and dev files (any version >=2.4 should be fine)](https://opencv.org/releases/)


## Results using [Rawzor dataset](https://imagecompression.info/test_images/)
Gray 8 bit images where used

Complete dataset:
![alt text](results_images/rawzor_complete.svg "LOCO-ANS configurations vs jpeg-ls and estimated entropy")


Natural images of the dataset:
![alt text](results_images/rawzor_natural.svg "LOCO-ANS configurations vs jpeg-ls and estimated entropy")

