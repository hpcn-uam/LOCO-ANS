#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cstdlib>
#include <libgen.h>

  int get_peak_error(const cv::Mat img0,const cv::Mat img1){
    cv::Mat tmp(img0.size(),img0.type());
    cv::absdiff(img0, img1, tmp);
    double min,max;
    cv::minMaxLoc(tmp, &min, &max);
    return int(max);
  }


int main(int argc, char const *argv[])
{
	if(argc <2) {
    printf("args:  img1_path  img2_path \n");
  }
  cv::Mat img_1 = cv::imread( argv[1],CV_LOAD_IMAGE_ANYDEPTH );
  if(img_1.empty()){
    std::cerr<<"empty file"<<std::endl;
    return 2;
  }

  cv::Mat img_2 = cv::imread( argv[2] ,CV_LOAD_IMAGE_ANYDEPTH);
  if(img_2.empty()){
    std::cerr<<"empty file"<<std::endl;
    return 2;
  }

  std::cout<<get_peak_error(img_1,img_2)<<std::endl;
	return 0;
}