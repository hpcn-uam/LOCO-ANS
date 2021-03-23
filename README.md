# LOCO-ANS CODEC 

This repository contains the source code resulting from the development of an ANS based coder for JPEG-LS.

When targeting the [Rawzor dataset](https://imagecompression.info/test_images/), the LOCO-ANS  achieves in mean up to 1.3\% bits per pixel reduction for lossless coding and 4.8\% for a maximum peak error set to 1, compared to JPEG-LS.
These gains continue to increase with the maximum peak error, showing in mean a 25\% better compression for a peak error set to 10. 
When considering only non-synthetic images, these improvement increase to 1.4\%, 5.8\% and 36.8\% for peak errors of 0, 1 and 10, respectively .
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

