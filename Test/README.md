# Quick test
The test takes an image as input and sweeping through different NEAR arguments obtains:
- estimated entropy (theoretical estimators and ideal coder)
- estimated bpp (practical estimators and ideal coder)
- actual bpp
- Max error verification

## Usage

``` bash
bash ./quick_test image_name min_error max_error
```

image_name (default "hdr"): Name of image from the image_dataset/gray8bit dataset to test (without extension).
min_error (default 0 ): initial NEAR
max_error (default 5 ): final NEAR

Images in the rawzor gray 8 bit dataset:
- artificial
- big_building
- big_tree
- bridge
- cathedral
- deer
- fireworks
- flower_foveon
- hdr
- leaves_iso_1600
- leaves_iso_200
- nightshot_iso_100
- nightshot_iso_1600
- spider_web
- zone_plate