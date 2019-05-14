// LDES.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <vector>
#include <array>
#include <fstream>
#include <sstream>
#include "ldes_tracker.h"
using namespace std;

void testKCF();
void testLDES();
void testPhaseCorrelation();

cv::Rect get_groundtruth(string& line);

int main()
{
	//testPhaseCorrelation();
	testLDES();
	//testKCF();
	return 0;
}

cv::Rect get_groundtruth(string& line) {
	std::stringstream ss;
	ss << line;
	string s;
	vector<int> v;
	while (std::getline(ss, s, ',')) {
		v.push_back(atoi(s.c_str()));
	}
	return cv::Rect(v[0], v[1], v[2], v[3]);
}

void testKCF() {
	string img_file = "D:/Dataset/OTB100/Mhyang.txt";
	string label_file = "D:/Dataset/OTB100/Mhyang_label.txt";
	LDESTracker tracker;

	ifstream fin, lfin;
	fin.open(img_file.c_str(), ios::in);
	lfin.open(label_file.c_str(), ios::in);

	int frameIndex = 0;
	cv::Rect new_pos;
	while (!fin.eof()) {
		string filename, gt;
		getline(fin, filename);
		getline(lfin, gt);
		if (gt.length() == 0)
			continue;

		cv::Mat image = cv::imread(filename);
		cv::Rect position = get_groundtruth(gt);
		if (frameIndex == 0){
			new_pos = tracker.testKCFTracker(image, position, true);
			new_pos = position;
		}
		else {
			new_pos = tracker.testKCFTracker(image, new_pos, false);
		}
		++frameIndex;

		cv::rectangle(image, new_pos, cv::Scalar(0, 0, 255), 2);
		cv::imshow("trackKCF", image);
		if (cv::waitKey(1) == 27)
			break;
	}
}

void testLDES() {
	string img_file = "D:/Dataset/OTB100/Mhyang.txt";
	string label_file = "D:/Dataset/OTB100/Mhyang_label.txt";
	LDESTracker tracker;

	ifstream fin, lfin;
	fin.open(img_file.c_str(), ios::in);
	lfin.open(label_file.c_str(), ios::in);

	int frameIndex = 0;
	cv::Rect new_pos;
	while (!fin.eof()) {
		string filename, gt;
		getline(fin, filename);
		getline(lfin, gt);

		cv::Mat image = cv::imread(filename);
		cv::Rect position = get_groundtruth(gt);
		if (frameIndex == 0) {
			tracker.init(position, image);
			new_pos = position;
		}
		else {
			tracker.updateModel(image,0);
			new_pos = tracker.cur_roi;
		}
		++frameIndex;

		cv::rectangle(image, new_pos, cv::Scalar(0, 0, 255), 2);
		cv::circle(image, tracker.cur_pos, 3, cv::Scalar(0, 255, 0), -1);
		cv::imshow("trackLDES", image);
		if (cv::waitKey(1) == 27)
			break;
	}
}

cv::Mat getHistFeatures(cv::Mat& img, int* size) {
	cv::Mat features(img.channels(), img.cols*img.rows, CV_32F);
	vector<cv::Mat > planes(3);
	cv::split(img, planes);
	planes[0].reshape(1, 1).copyTo(features.row(0));
	planes[1].reshape(1, 1).copyTo(features.row(1));
	planes[2].reshape(1, 1).copyTo(features.row(2));
	size[0] = img.rows;
	size[1] = img.cols;
	size[2] = img.channels();
	return features;
}

void testPhaseCorrelation() {
	cv::Rect roi(84, 53, 62, 70);
	LDESTracker tracker;
	float padding = 2.5;
	int sz = 120;

	int window_sz = static_cast<int>(sqrt(roi.area())*padding);
	int cx = (int)(roi.x + roi.width*0.5);
	int cy = (int)(roi.y + roi.height*0.5);

	cv::Rect window(cx - window_sz / 2, cy - window_sz / 2, window_sz, window_sz);

	string img1_path = "./0001.jpg";

	cv::Mat image = cv::imread(img1_path);
	cv::Mat img1 = image(window).clone();
	cv::Mat img2;
	cv::resize(img1, img1, cv::Size(sz, sz));

	float last_scale = 1.1;
	float cur_scale = 0.85;
	cv::Mat rot_matrix = cv::getRotationMatrix2D(cv::Point2f(cx, cy), -27.6, cur_scale);
	rot_matrix.convertTo(rot_matrix, CV_32F);
	rot_matrix.at<float>(0, 2) += window_sz * last_scale * 0.5 - cx;
	rot_matrix.at<float>(1, 2) += window_sz * last_scale * 0.5 - cy;

	cv::warpAffine(image, img2, rot_matrix, cv::Size(window_sz*last_scale,window_sz*last_scale));
	cv::resize(img2, img2, cv::Size(sz, sz));	//cannot resize
	//cv::blur(img2, img2, cv::Size(5, 5));
	//cv::Mat mask(20, 60, CV_8UC3, cv::Scalar(0));
	//mask.copyTo(img2(cv::Rect(10, 40, mask.cols, mask.rows)));
	cv::imshow("img1", img1);
	cv::imshow("img2", img2);

	cv::Mat log1, log2;

	//float mag= sz / (log(sqrt((sz*sz + sz*sz)*0.25)));
	float mag = 30;
	cv::logPolar(img1, log1, cv::Point2f(0.5*sz, 0.5*sz), mag, cv::INTER_LINEAR);
	cv::logPolar(img2, log2, cv::Point2f(0.5*sz, 0.5*sz), mag, cv::INTER_LINEAR);

	int size[3] = { 0 };

	//better with larger featuremap
	bool _hog = false;

	cv::Mat x1, x2;
	if (_hog) {
		cv::Mat _empty;
		x1 = tracker.getFeatures(log1, _empty, size, false);
		x2 = tracker.getFeatures(log2, _empty, size, false);
	}
	else {
		x1 = getHistFeatures(log1, size);
		x2 = getHistFeatures(log2, size);
	}
	float _scale = 2.0;
	cv::Mat rf=phaseCorrelation(x1, x2, size[0], size[1], size[2]);
	cv::Mat res = fftd(rf, true);
	rearrange(res);
	
	cv::resize(res, res, cv::Size(0, 0), _scale, _scale, cv::INTER_LINEAR);

	size[0] *= _scale;
	size[1] *= _scale;
	cv::Rect center(5, 5, size[1] - 10, size[0] - 10);
	res = res(center);
	cv::Point2i pi;
	
	double pv;
	cv::minMaxLoc(res, NULL, &pv, NULL, &pi);
	// More precise without subpixel!!
	//if(pi.x > 1 && pi.x < res.cols - 1) {
	//	pi.x += tracker.subPixelPeak(res.at<float>(pi.y, pi.x - 1), pv, res.at<float>(pi.y, pi.x + 1));
	//}

	//if (pi.y > 1 && pi.y < res.rows - 1) {
	//	pi.y += tracker.subPixelPeak(res.at<float>(pi.y - 1, pi.x), pv, res.at<float>(pi.y + 1, pi.x));
	//}
	pi.x += 5;
	pi.y += 5;
	pi.x -= size[1]*0.5;
	pi.y -= size[0]*0.5;

	pi.x /= _scale;
	pi.y /= _scale;
	size[1] /= _scale;
	float rot = -(pi.y) * 180.0 / (size[1] * 0.5);
	float scale = exp((pi.x)/mag);

	cout << "rot: " << rot << ", scale: " << 1.0*last_scale*scale << endl;
	cv::normalize(res, res, 0, 1, cv::NORM_MINMAX);
	cv::imshow("log1", log1);
	cv::imshow("log2", log2);
	cv::imshow("res", res);
	cv::waitKey();
}