/**
* Author: Liam Farrelly
* MeanShift Segmentation and floodFill postprocess from: https://sourceforge.net/p/emgucv/opencv/ci/3a873cb051efd9126e7dd05e3b47b046a0998dee/tree/samples/cpp/meanshift_segmentation.cpp#l16
*
*/

#include "stdafx.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include <map>
#include <iostream>
#include <fstream>

using namespace cv;
using namespace std;

/// Global variables
Mat src, dst;
Mat grayscale, detected_edges;

int morph_elem = 0;
int morph_size = 0;
int morph_operator = 0;
int const max_operator = 4;
int const max_elem = 2;
int const max_kernel_size = 21;
bool kmeansBtn = false;

//CV_RETR_TREE returns contours as hierarchy 2D array, each contour is: [NEXT_C, PREV_C, CHILD_C, PARENT_C]
const int NEXT = 0;
const int PREV = 1;
const int CHILD = 2;
const int PARENT = 3;

const vector<vector<Rect>> groundTruths = { /*NOTICE1*/ { Rect(Point(34,17),Point(286,107)), Rect(Point(32,117),Point(297,223)), Rect(Point(76,234),Point(105,252)) },
                                            /*NOTICE2*/ { Rect(Point(47,191),Point(224,253)) },
											/*NOTICE3*/ { Rect(Point(142,121),Point(566,392)) },
											/*NOTICE4*/ { Rect(Point(157,72),Point(378,134)), Rect(Point(392,89),Point(448,132)), Rect(Point(405,138),Point(442,152)), Rect(Point(80,157),Point(410,245)), Rect(Point(82,258),Point(372,322)) },
											/*NOTICE5*/ { Rect(Point(112,73),Point(598,170)), Rect(Point(108,178),Point(549,256)), Rect(Point(107,264),Point(522,352)) },
											/*NOTICE6*/ { Rect(Point(91,54),Point(446,227)) },
											/*NOTICE7*/ { Rect(Point(64,64),Point(476, 268)), Rect(Point(529,126),Point(611,188)), Rect(Point(545,192),Point(603,211)), Rect(Point(210,305),Point(595,384)) },
											/*NOTICE8*/ { Rect(Point(158,90),Point(768,161)), Rect(Point(114,174),Point(800,279)) }
										  };



int spatialRad, colorRad, maxPyrLevel, contrastConst;
Mat img, res, heatMap, groundTruth;

const char* msWin = "meanshift";
const char* groundTruthWindow = "Ground Truths";
const char* window_name = "Untampered image";
const char* canny_window = "Canny image";
int lowThreshold,kernel_size;

/** Function Headers */
vector<Rect> meanShiftSegmentation(Mat&, Mat&);
vector<Rect> floodFillPostprocess(Mat&, const Scalar&);
void getHistogram(Mat&);
void toGreyscale(Mat&, Mat&);
void cannyThreshold(Mat&, Mat&);
void boundedContours(Mat&, Mat&);
void contourDraw(Mat&, vector<vector<Point>>, vector<Vec4i>, int);
Mat KMeans_Clustering(Mat&);
vector<Rect> getContourROI(Mat&, vector<Rect>);
int countChildren(int, vector<Vec4i>);
bool isSubset(Rect, Rect);
bool hasOverlap(Rect, Rect);
Rect mergeRects(Rect, Rect);
vector<Rect> roiCompress(vector<Rect>, int, Rect&, Rect&);
void compareCandidates(vector<Rect>&, Mat&);
double calcDICE(vector<Rect>, vector<Rect>);

//void Morphology_Operations(int, void*);

/*
Kmeans -> greyscale -> contours -> book page 69/70 -> find connected regions -> labeled -> analyse regions to try find text
*/


/**
* @function main
*/
int main(int, char** argv)
{

	ofstream scoresFile("../data/DICE-Scores.txt");
	
	string results = "";
	double totalDice = 0;
	for (int i = 0; i < 8; i++) {
		string num = to_string(i+1);
		results += num + ": ";
		img = imread("../data/Notice"+num+".jpg", IMREAD_COLOR); // Load an image
		img.copyTo(groundTruth);

		/*int imgNum = stoi(str.substr(14,1));
		for (int i = str.length()-1; i >= 0; i--) {
			if (isdigit(str[i])) {
				//convert ascii code to number
				imgNum = str[i] - '0';
				//exit loop
				i = -1;
			}
		}*/

		if (img.empty())
		{
			return -1;
		}

		/// Default start
		spatialRad = 8;
		colorRad = 30;
		maxPyrLevel = 1;
		contrastConst = 1.4;
		kernel_size = 3;

		namedWindow(msWin, CV_WINDOW_AUTOSIZE);
		/*namedWindow(window_name, CV_WINDOW_AUTOSIZE);
		namedWindow(canny_window, CV_WINDOW_AUTOSIZE);
		namedWindow(groundTruthWindow, CV_WINDOW_AUTOSIZE);*/

		//imshow(window_name, img);

		Mat temp;
		img.copyTo(temp);

		//KMeans_Clustering(0, 0);

		Mat reducedColour;
		//reducedColour = KMeans_Clustering(img);
		vector<Rect> segments = meanShiftSegmentation(temp, reducedColour);
		//imshow(window_name, img);
		//increaseContrast(0, 0
		

		//imshow(msWin, reducedColour);
		//imshow(canny_window, temp);

		toGreyscale(reducedColour, reducedColour);
		threshold(reducedColour, reducedColour, 0, 255, THRESH_BINARY + THRESH_OTSU);
		
		segments = getContourROI(reducedColour, segments);
		
		vector<Rect> gtRegions = groundTruths.at(i);
		for (int i = 0; i < gtRegions.size(); i++) {
			rectangle(groundTruth, gtRegions.at(i), CV_RGB(255, 0, 0));
		}
		for (int i = 0; i < segments.size(); i++) {
			rectangle(groundTruth, segments.at(i), CV_RGB(0, 0, 255), 3);
		}

		imshow(groundTruthWindow, groundTruth);

		double diceScore = calcDICE(gtRegions, segments);
		cout << "DICE: " << diceScore << endl;
		totalDice += diceScore;
		if (i == 6) {
			cout << "avg bar final : " << totalDice / 7;
		}
		results += to_string(diceScore) + "\n";
			

		//waitKey();
	}
	if (scoresFile.is_open()) {
		scoresFile << results;
		scoresFile << to_string(totalDice/8);
		scoresFile.close();
	}
	return 0;
}

double calcDICE(vector<Rect> groundTruths, vector<Rect> observed) {
	double areaOfOverlap = 0;
	double totalAreaGT = 0;
	double totalAreaOb = 0;
	for (int i = 0; i < groundTruths.size(); i++) {
		for (int j = 0; j < observed.size(); j++) {
			areaOfOverlap += (groundTruths.at(i) & observed.at(j)).area();
			if (i == 0) {
				totalAreaOb += observed.at(j).area();
			}
		}
		totalAreaGT += groundTruths.at(i).area();
	}
	//cout << "area of overlap: " << areaOfOverlap << endl;
	//cout << "total area ob : " << totalAreaOb << " total area gt: " << totalAreaGT << " total total " << totalAreaGT + totalAreaOb << endl;
	return (2*areaOfOverlap) / (totalAreaGT+totalAreaOb);
}

vector<Rect> floodFillPostprocess(Mat& target, const Scalar& colorDiff = Scalar::all(2))
{
	CV_Assert(!target.empty());
	RNG rng = theRNG();
	Mat mask(target.rows + 2, target.cols + 2, CV_8UC1, Scalar::all(0));

	vector<Rect> segmentBoundaries;
	vector<Point> roiMarkers;
	for (int y = 0; y < target.rows; y++)
	{
		for (int x = 0; x < target.cols; x++)
		{
			if (mask.at<uchar>(y + 1, x + 1) == 0)
			{
				//get rgb values for this point
				Point3_<uchar>* p = target.ptr<Point3_<uchar> >(y, x);
				Rect segmentBoundary;
				Scalar newVal(p->x, p->y, p->z);
				//Scalar newVal(150, 150, 150);
				int connectivity = 8;
				floodFill(target, mask, Point(x, y), newVal, &segmentBoundary, colorDiff, colorDiff, connectivity); //| CV_FLOODFILL_MASK_ONLY);
				//cout << result << " @ " << x << "," << y << endl;
				if ((segmentBoundary.height > 5 && segmentBoundary.width > 5) &&
					(segmentBoundary.height < target.rows / 4 && segmentBoundary.width < target.cols / 4)) {
					segmentBoundaries.push_back(segmentBoundary);
					
					roiMarkers.push_back(segmentBoundary.tl());
					roiMarkers.push_back(segmentBoundary.br());
				}
			}
			//cout << "mask = " << mask.at<uchar>(y + 1, x + 1) << " @ " << y + 1 << "," << x + 1 << endl;
		}
	}
	/*double nSegmentBoundaries = double(segmentBoundaries.size());
	//double transparency = nSegmentBoundaries/(2*nSegmentBoundaries);
	double transparency = 5 / nSegmentBoundaries;
	cout << transparency << endl;
	Mat temp;
	img.copyTo(temp);
	//img.copyTo(grey);
	for (int i = 0; i < segmentBoundaries.size(); i++) {
		Rect thisSegBoundary = segmentBoundaries.at(i);
		rectangle(temp, thisSegBoundary, CV_RGB(255, 0, 0), CV_FILLED);
		addWeighted(temp, transparency, img, (1 - transparency), 0, temp);
	}*/
	/*rectangle(temp, boundingRect(roiMarkers), CV_RGB(255, 0, 0));
	namedWindow("roi", CV_WINDOW_AUTOSIZE);
	imshow("roi", temp);*/
	return segmentBoundaries;
}

//given a binary image, function returns regions of interest
vector<Rect> getContourROI(Mat& image, vector<Rect> segments) {
	vector<vector<Point>> allContours;
	vector<Vec4i> hierarchy;
	findContours(image, allContours, hierarchy, CV_RETR_TREE, CHAIN_APPROX_NONE);
	

	//vector<vector<Point>> roi;
	//contours of interest are contours with a high number of children
	int i = 1;
	//cout << hierarchy.size() << endl;
	namedWindow("contour test", CV_WINDOW_AUTOSIZE);
	//cout << countChildren(i, hierarchy) << endl;
	int maxC = 0;
	int secondMaxC = 0;
	int indexOfInterest = 0;
	int secondIndexOfInterest = 0;
	Mat blank(image.rows, image.cols, CV_8UC3);
	img.copyTo(blank);
	for (; i < hierarchy.size(); i++) {
		int c = countChildren(i, hierarchy);
		if (c > maxC) {
			secondIndexOfInterest = indexOfInterest;
			indexOfInterest = i;
			secondMaxC = maxC;
			maxC = c;
			cout << "**NEW MAX @ " << i << " of " << c << endl;
		}
		else if (c > secondMaxC) {
			secondIndexOfInterest = i;
			secondMaxC = c;
			cout << "!!!! NEW 2 @ " << i << " of " << c << endl;
		}
		//drawContours(blank, allContours, i, 155);
		//imshow("contour test", blank);
		//waitKey(0);
	}

	/*Extract ROI marked by the top 2 contours from the original image, store in vector of rois*/
	//vector<Mat> regionsOfInterest;
	Mat mask = Mat::zeros(img.size(), CV_8UC1);
	drawContours(mask, allContours, indexOfInterest, Scalar(255), CV_FILLED);
	Mat contourRegion;
	img.copyTo(contourRegion, mask);
	Rect firstRoi = boundingRect(allContours.at(indexOfInterest));


	mask = Mat::zeros(img.size(), CV_8UC1);
	drawContours(mask, allContours, secondIndexOfInterest, Scalar(255), CV_FILLED);
	Mat secondContourRegion;
	img.copyTo(secondContourRegion, mask);
	Rect secondRoi = boundingRect(allContours.at(secondIndexOfInterest));

	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < segments.size(); j++) {
			segments = roiCompress(segments, j, firstRoi, secondRoi);
		}
		cout << "n segs : " << segments.size() << endl;
	}

	/*vector<Mat> textCandidates;
	for (int i = 0; i < segments.size(); i++) {
		textCandidates.push_back(Mat(image,segments.at(i)));
	}*/
	
	compareCandidates(segments, image);

	return segments;
}

bool isSubset(Rect r1, Rect r2) {
	int larger = r1.area() > r2.area() ? r1.area() : r2.area();
	return (r1 | r2).area() == larger;
}

bool hasOverlap(Rect r1, Rect r2) {
	if (isSubset(r1, r2)) {
		return true;
	}
	return (r1 & r2).area() > 0;
}

Rect mergeRects(Rect r1, Rect r2) {
	return (r1 | r2);
}

vector<Rect> roiCompress(vector<Rect> roiRectangles, int i, Rect& roi1, Rect& roi2) {
	vector<Rect> compressed;
	
	Rect focalRect = roiRectangles.at(i);
	for (int j = 0; j < roiRectangles.size(); j++) {
		if (j != i) {
			Rect r2 = roiRectangles.at(j);
			if (isSubset(roi1, r2) || isSubset(roi2, r2)) {
				Point r2tr = Point(r2.tl().x + r2.width, r2.tl().y);
				Point focalRecttr = Point(focalRect.tl().x + focalRect.width, focalRect.tl().y);

				Point r2bl = Point(r2.br().x - r2.width, r2.br().y);
				Point focalRectbl = Point(focalRect.br().x - focalRect.width, focalRect.br().y);

				Point r2Ctr = (r2.tl() + r2.br())*0.5;
				Point focalRectCtr = (focalRect.tl() + focalRect.br())*0.5;

				Point r2CtrL = Point(r2Ctr.x - (r2.width / 2), r2Ctr.y);
				Point focalRectCtrL = Point(focalRectCtr.x - (focalRect.width / 2), focalRectCtr.y);

				double minDistHoriz = min(norm(focalRect.tl() - r2tr), norm(focalRecttr - r2.tl()));
				double minDistVert = min(norm(focalRectCtr - r2Ctr), min(norm(focalRectbl - r2.tl()), norm(focalRect.br() - r2tr)));
				double distCtrs = norm(focalRectCtr.x - r2Ctr.x);

				//line spacing on signs tends to be a function of font size(region height), metric to account for this
				Point focalRectVShift = Point(focalRectCtr.x, focalRectCtr.y - focalRect.height);
				Point r2VShift = Point(r2Ctr.x, r2Ctr.y - r2.height);
				bool isNextLineDown = r2.contains(focalRectVShift) || focalRect.contains(r2VShift);

				if (hasOverlap(focalRect, r2) || minDistVert < 30 || minDistHoriz < 30) {
					Rect merged = mergeRects(focalRect, r2);
					double totalArea = focalRect.area() + r2.area();
					//calculate potential increase in area of the focal rectangle
					double proposedIncrease = ((merged.area() / totalArea) * 100) - 100;
					if (proposedIncrease < 30) {
						focalRect = merged;
					}
					else {
						compressed.push_back(r2);
					}
				}
				else if (isNextLineDown) {
					focalRect = mergeRects(focalRect, r2);
				}
				else {
					if (!isSubset(focalRect, r2)) {
						compressed.push_back(r2);
					}
				}
			}
		}
	}
	compressed.push_back(focalRect);

	return compressed;
}

int countChildren(int index, vector<Vec4i> hierarchy) {
	int total = 0;
	for (int j = hierarchy[index][CHILD]; j >= 0; j = hierarchy[j][NEXT]) {
		total += countChildren(j, hierarchy) + 1;
	}
	return total;
}

void toGreyscale(Mat& image, Mat& output) {
	cvtColor(image, output, CV_BGR2GRAY);

	//imwrite("../data/GreyPostMSCont2.jpg", output);

	//namedWindow("greyscale", CV_WINDOW_AUTOSIZE);
	//imshow("greyscale", output);
}

vector<Rect> meanShiftSegmentation(Mat& target, Mat& output)
{
	
	cout << "spatialRad=" << spatialRad << "; "
		<< "colorRad=" << colorRad << "; "
		<< "maxPyrLevel=" << maxPyrLevel << endl;
	pyrMeanShiftFiltering(target, output, spatialRad, colorRad, maxPyrLevel);
	
	vector<Rect> segments = floodFillPostprocess(output, Scalar::all(2));
	
	return segments;
}

void compareCandidates(vector<Rect>& candidates, Mat& image) {
	//image is a binary image
	namedWindow("candidate comparison", CV_WINDOW_AUTOSIZE);
	vector<Rect>::iterator myIterator;
	for (myIterator = candidates.begin(); myIterator != candidates.end(); ) {
		vector<vector<Point>> allContours;
		vector<Vec4i> hierarchy;
		findContours(Mat(image, *myIterator), allContours, hierarchy, CV_RETR_TREE, CHAIN_APPROX_NONE);

		int contourCount = hierarchy.size();
		cout << "n contours : " << contourCount << endl;
		if (contourCount < 5) {
			myIterator = candidates.erase(myIterator);
			cout << contourCount << " element deleted" << endl;
		}
		else {
			//imshow("candidate comparison", Mat(image, *myIterator));
			myIterator++;
		}

		//waitKey();
	}

}

//Taken from https://docs.opencv.org/2.4/doc/tutorials/imgproc/histograms/histogram_calculation/histogram_calculation.html
void getHistogram(Mat& src) {

	if (!src.data)
	{
		return;
	}

	/// Separate the image in 3 places ( B, G and R )
	vector<Mat> bgr_planes;
	split(src, bgr_planes);

	/// Establish the number of bins
	int histSize = 256;

	/// Set the ranges ( for B,G,R) )
	float range[] = { 0, 256 };
	const float* histRange = { range };

	bool uniform = true; bool accumulate = false;

	Mat b_hist, g_hist, r_hist;
	Mat grayscale, gray_hist;
	cvtColor(img, grayscale, CV_BGR2GRAY);

	//namedWindow("greyscale", CV_WINDOW_AUTOSIZE);
	//imshow("greyscale", grayscale);

	/// Compute the histograms:
	calcHist(&bgr_planes[0], 1, 0, Mat(), b_hist, 1, &histSize, &histRange, uniform, accumulate);
	calcHist(&bgr_planes[1], 1, 0, Mat(), g_hist, 1, &histSize, &histRange, uniform, accumulate);
	calcHist(&bgr_planes[2], 1, 0, Mat(), r_hist, 1, &histSize, &histRange, uniform, accumulate);

	calcHist(&grayscale, 1, 0, Mat(), gray_hist, 1, &histSize, &histRange, uniform, accumulate);

	// Draw the histograms for B, G and R
	int hist_w = 512; int hist_h = 400;
	int bin_w = cvRound((double)hist_w / histSize);

	Mat histImage(hist_h, hist_w, CV_8UC3, Scalar(0, 0, 0));

	Mat grayHistImage(hist_h, hist_w, CV_8UC3, Scalar(0, 0, 0));

	/// Normalize the result to [ 0, histImage.rows ]
	normalize(b_hist, b_hist, 0, histImage.rows, NORM_MINMAX, -1, Mat());
	normalize(g_hist, g_hist, 0, histImage.rows, NORM_MINMAX, -1, Mat());
	normalize(r_hist, r_hist, 0, histImage.rows, NORM_MINMAX, -1, Mat());

	normalize(gray_hist, gray_hist, 0, grayHistImage.rows, NORM_MINMAX, -1, Mat());

	/// Draw for each channel
	for (int i = 1; i < histSize; i++)
	{
		line(histImage, Point(bin_w*(i - 1), hist_h - cvRound(b_hist.at<float>(i - 1))),
			Point(bin_w*(i), hist_h - cvRound(b_hist.at<float>(i))),
			Scalar(255, 0, 0), 2, 8, 0);
		line(histImage, Point(bin_w*(i - 1), hist_h - cvRound(g_hist.at<float>(i - 1))),
			Point(bin_w*(i), hist_h - cvRound(g_hist.at<float>(i))),
			Scalar(0, 255, 0), 2, 8, 0);
		line(histImage, Point(bin_w*(i - 1), hist_h - cvRound(r_hist.at<float>(i - 1))),
			Point(bin_w*(i), hist_h - cvRound(r_hist.at<float>(i))),
			Scalar(0, 0, 255), 2, 8, 0);

		line(grayHistImage, Point(bin_w*(i - 1), hist_h - cvRound(gray_hist.at<float>(i - 1))),
			Point(bin_w*(i), hist_h - cvRound(gray_hist.at<float>(i))),
			Scalar(127, 127, 127), 2, 8, 0);
	}

	/// Display
	//namedWindow("calcHist Demo", CV_WINDOW_AUTOSIZE);
	//imshow("calcHist Demo", histImage);

	//namedWindow("greyscale hist", CV_WINDOW_AUTOSIZE);
	//imshow("greyscale hist", grayHistImage);

}


//Canny edge detection, code from OpenCV: https://docs.opencv.org/2.4/doc/tutorials/imgproc/imgtrans/canny_detector/canny_detector.html
void cannyThreshold(Mat& target, Mat& output)
{
	/// Reduce noise with a kernel 3x3
	GaussianBlur(target, output, Size(3, 3),0, 0);

	/// Canny detector
	int ratio = 3, lowThreshold = 20;
	Canny(output, output, lowThreshold, lowThreshold*ratio, kernel_size);

	/// Using Canny's output as a mask, we display our result
	dst = Scalar::all(0);

	//cout << "Edges: " << detected_edges << endl;

	target.copyTo(dst, output);
	//imwrite("../data/cannyImage.jpg", dst);

	output = dst;
}

void boundedContours(Mat& contourImg, Mat& output) {

	adaptiveThreshold(contourImg, contourImg, 255, CV_ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 5, 0);
	Mat blurred;
	GaussianBlur(contourImg, blurred, Size(3, 3), 0, 0);
	threshold(blurred, blurred, 0, 255, THRESH_BINARY + THRESH_OTSU);

	int kernelDim = 2;
	Mat kernel = getStructuringElement(0, Size(kernelDim,kernelDim), Point(kernelDim/2,kernelDim/2));
	morphologyEx(blurred, blurred, MORPH_OPEN, kernel);
	morphologyEx(blurred, blurred, MORPH_CLOSE, kernel);

	namedWindow("binary", CV_WINDOW_AUTOSIZE);
	imshow("binary", blurred);
	imwrite("../data/binary.jpg", blurred);
	
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;
	findContours(blurred, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE);

	//vector<Rect> boundaries(contours.size());
	contourDraw(output, contours, hierarchy, 0);
}

void contourDraw(Mat& img, vector<vector<Point>> contours, vector<Vec4i> hierarchy, int startPoint) {

	/*const int HIERARCHY_NEXT = 0;
	const int HIERARCHY_PREV = 1;
	const int HIERARCHY_CHILD = 2;
	const int HIERARCHY_PARENT = 3;*/

	vector<Rect> boundaries;
	for (int i = 0; i < contours.size(); i++) {
		int r = rand() % 255;
		int g = rand() % 255;
		int b = rand() % 255;
		//drawContours(img, contours, i, CV_RGB(r, g, b));
		Rect bounder = boundingRect(contours[i]);
		rectangle(img, bounder.tl(), bounder.br(), CV_RGB(r,g,b), CV_FILLED, 8, 0);
		/*for (int j = hierarchy[i][HIERARCHY_CHILD]; j >= 0; j = hierarchy[j][HIERARCHY_NEXT]) {
			boundaries[j] = boundingRect(contours[j]);
			rectangle(img, boundaries[j].tl(), boundaries[j].br(), CV_RGB(r, g, b), 2, 8, 0);
			//contourDraw(img, contours, hierarchy, hierarchy[j][HIERARCHY_CHILD]);
		}*/
	}

}

/*
try this with if rect1.br - (rect2.tl+5) > 0 then merge

if((rect1 & rect2) == rect1) ... // rect1 is completely inside rect2; do nothing.
else if((rect1 & rect2).area() > 0) // they intersect; merge them.
newrect = rect1 | rect2;
... // remove rect1 and rect2 from list and insert newrect.
*/



//![morphology_operations]
/**
* @function Morphology_Operations
*/
/*void Morphology_Operations(int, void*)
{
// Since MORPH_X : 2,3,4,5 and 6
//![operation]
int operation = morph_operator + 2;
//![operation]

Mat element = getStructuringElement(morph_elem, Size(2 * morph_size + 1, 2 * morph_size + 1), Point(morph_size, morph_size));

/// Apply the specified morphology operation
morphologyEx(src, dst, operation, element);
imshow(window_name, dst);
}*/
//![morphology_operations]


Mat KMeans_Clustering(Mat& target)
{


		//Store the image pixels as an array of samples
		Mat samples(target.rows*target.cols, 3, CV_32F);
		float* sample = samples.ptr<float>(0);
		for (int row = 0; row < target.rows; row++) {
			for (int column = 0; column < target.cols; column++) {
				for (int channel = 0; channel < 3; channel++) {
					samples.at<float>(row*target.cols + column, channel) = (uchar)target.at<Vec3b>(row, column)[channel];
				}
			}
		}
		//Apply kmeans clustering, determining the cluster centres and a label for each pixel
		Mat labels, centres;
		kmeans(samples, 5, labels, TermCriteria(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 100, 0.0001), 10, KMEANS_PP_CENTERS, centres);
		//use centers and labels to populate destination image
		Mat dest = Mat(target.size(), target.type());
		for (int row = 0; row < target.rows; row++) {
			for (int col = 0; col < target.cols; col++) {
				for (int channel = 0; channel < 3;channel++) {
					dest.at<Vec3b>(row, col)[channel] = (uchar)centres.at<float>(*(labels.ptr<int>(row*target.cols + col)), channel);
				}
			}
		}
		return dest;
}