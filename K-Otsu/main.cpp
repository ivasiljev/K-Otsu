#include <iostream>
#include <stdio.h>
#include <vector>
#include <algorithm>

extern "C"
{
#include "jpeglib.h"
}

struct ImageData {
	unsigned char *pixels;
	int  width;
	int height;
	char components;
};

struct rgb
{
	int r;
	int g;
	int b;
};

std::vector<rgb> colors = {
	{ 255, 0, 0 },
	{ 0, 255, 0 },
	{ 0, 0, 255 },
	{ 125, 125, 125 },
	{ 255, 255, 0 },
	{ 0, 255, 255 },
	{ 255, 0, 255 }
};

ImageData *imageData;
std::vector<int> hist;
std::vector<std::vector<std::pair<double, std::vector<int> > > > d;
std::vector<int> thresholds;

int fileInputJpeg(const char *fileInput)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int row_stride;

	FILE * infile;
	fopen_s(&infile, fileInput, "rb");
	if (!infile)
	{
		printf("Can't open %s", fileInput);
		return 1;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	row_stride = cinfo.output_width * cinfo.output_components;

	JSAMPROW buffer[1];

	imageData = new ImageData;
	imageData->width = cinfo.output_width;
	imageData->height = cinfo.output_height;
	imageData->components = cinfo.output_components;
	imageData->pixels = new unsigned char[cinfo.output_width * cinfo.output_height * cinfo.output_components];

	int counter = 0;
	while (cinfo.output_scanline < cinfo.output_height) {
		buffer[0] = imageData->pixels + counter;
		jpeg_read_scanlines(&cinfo, buffer, 1);
		counter += row_stride;
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	fclose(infile);
	return 0;
}

int fileOutputJpeg(char fileOutput[])
{
	int quality = 100;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	FILE * outfile;
	fopen_s(&outfile, fileOutput, "wb");
	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = imageData->width;
	cinfo.image_height = imageData->height;
	cinfo.input_components = imageData->components;

	if (imageData->components == 3)
		cinfo.in_color_space = JCS_RGB;
	else if (imageData->components == 1)
		cinfo.in_color_space = JCS_GRAYSCALE;
	else
	{
		printf("Something wrong(fileOutputJpeg)\n");
		return 2;
	}

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, true);
	jpeg_start_compress(&cinfo, 1);

	JSAMPROW row_pointer[1];
	int row_stride = cinfo.image_width * imageData->components;
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = (JSAMPLE *)(imageData->pixels + cinfo.next_scanline * row_stride);
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	fclose(outfile);
	return 0;
}

unsigned char findIntensity(unsigned char *color)
{
	int sum = 0;
	for (int i = 0; i < imageData->components; i++)
	{
		sum += *(color + i);
	}
	return (unsigned char)((double)sum / imageData->components);
}

void createIntensityHist(int countIntensLevels)
{
	hist.resize(countIntensLevels);
	for (int i = 0; i < countIntensLevels; i++)
		hist[i] = 0;

	for (int i = 0; i < imageData->height; i++)
		for (int j = 0; j < imageData->width; j++)
		{
			unsigned char *color = imageData->pixels + i * imageData->width * imageData->components + j * imageData->components;
			unsigned char intensity = findIntensity(color);

			hist[intensity]++;
		}
}

double wcv(int k, int n)
{
	double w = 0;
	double m = 0;
	double disp = 0;
	for (int i = k; i <= n; i++)
	{
		w += hist[i];
		m += i * hist[i];
	}
	if (w == 0)
		return 0;
	m = m / w;
	for (int i = k; i <= n; i++)
	{
		disp += (i - m) * (i - m) * hist[i];
	}
	return disp;
}

std::pair<double, std::vector<int>> dp(int n, int m, std::vector<int> path)
{
	if (m == 1)
	{
		path.push_back(n);
		d[n][1].second = path;
		return d[n][1];
	}
	else
	{
		for (int k = n; k > m - 1; k--)
		{
			double d_km;
			std::vector<int> newPath;
			if (d[k - 1][m - 1].first == -1)
			{
				std::pair<double, std::vector<int>> buf = dp(k - 1, m - 1, path);
				d_km = buf.first;
				newPath = buf.second;
				newPath.insert(newPath.begin(), k);
			}
			else
			{
				d_km = d[k - 1][m - 1].first;
				newPath = d[k - 1][m - 1].second;
				newPath.insert(newPath.begin(), k);
			}
			double newVal = wcv(k, n) + d_km;
			
			if (newVal < d[n][m].first || d[n][m].first == -1)
			{
				d[n][m].first = newVal;
				d[n][m].second = newPath;
			}
		}
		return d[n][m];
	}
}

void kOtsu(int k)
{
	createIntensityHist(256);

	d.assign(256, std::vector<std::pair<double, std::vector<int> > >(256, std::make_pair(-1, std::vector<int> (1))));
	for (int i = 0; i < 256; i++)
		d[i][1].first = wcv(0, i);

	thresholds = dp(255, k, std::vector<int>(1)).second;
	std::sort(thresholds.begin(), thresholds.end());
	for (int i = 0; i < thresholds.size(); i++)
	{
		std::cout << thresholds[i] << ' ';
	}
	std::cout << '\n';
}

void segmentation()
{
	unsigned char *color;
	for (int i = 0; i < imageData->height; i++)
		for (int j = 0; j < imageData->width; j++)
		{
			color = imageData->pixels + i * imageData->width * imageData->components + j * imageData->components;
			unsigned char intensity = findIntensity(color);

			for (int i = thresholds.size() - 1; i >= 0; i--)
			{
				if (intensity >= thresholds[i])
				{
					*color = colors[i].r; color++;
					*color = colors[i].g; color++;
					*color = colors[i].b;
					break;
				}
			}
		}
}

int main()
{
	printf("Started\n");

	const char inFile[] = "Penguins.jpg";

	int err = fileInputJpeg(inFile);
	if (err == 1)
		return err;

	kOtsu(6);
	segmentation();

	char outFile[255];
	snprintf(outFile, 255, "%s%s", "Result_", inFile);
	err = fileOutputJpeg(outFile);
	if (err == 2)
		return err;

	free(imageData->pixels);
	free(imageData);
	system("pause");
	return 0;
}