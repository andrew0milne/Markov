
#include <iostream>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <list>
#include <vector>
#include <set>
#include <windows.h>
#include <string>
#include <conio.h>
#include <ctype.h>

#include <random>

#include "GdiplusHeaderFunction.h"
#include <gdiplus.h>

using namespace Gdiplus;
using namespace std;

#pragma comment (lib,"Gdiplus.lib")

//The point that scrolling occurs
#define SCROLL_POS   39					

//Holds screen info
CONSOLE_SCREEN_BUFFER_INFO con_info;

//Handle to console
HANDLE hconsole;

// Used to represent a pixel
struct Pixel
{
	int r = 0;
	int g = 0;
	int b = 0;

	Color colour;

	void SetColour(Color col)
	{
		colour = col;
		r = col.GetRed();
		g = col.GetGreen();
		b = col.GetBlue();
	}
};

// Used to store the information for a tile
struct Tile
{
	vector<vector<Pixel>> tile;
	Color colour;
	int id = 0;
};

// Stores map data and tile list for passing between functions
struct InitOutput
{
	vector<Tile> tile_list;
	vector<vector<int>> map;
};

// Used to stores data for frequency distributions
struct tile_holder_3
{
	vector<int> neighbours_count;
	
	int neighbour[3][3] =
	{
		{ -1,-1,-1 },
		{ -1,-1,-1 },
		{ -1,-1,-1 }
	};


	int total_count = 0;

	void Init(int number_of_tiles)
	{
		for (int i = 0; i < number_of_tiles; i++)
		{
			neighbours_count.push_back(0);
		}
	}

	void CountTile(int here)
	{
		neighbours_count[here]++;
		total_count++;
	}
};

int d_matrix[3][3] =
{
	{ 0,0,0 },
	{ 0,1,1 },
	{ 1,1,2 }
};

// Converts input filename to a wchar_t
// From: http://www.cplusplus.com/forum/general/84768/
wchar_t* GetFileName(string filename, string file_type)
{
	string name = filename + file_type;
	wchar_t* wcstring = new wchar_t[name.length() + 1];
	copy(name.begin(), name.end(), wcstring);
	wcstring[name.length()] = 0;

	return wcstring;
}

// Saves the tile list to a .png
void SaveTileBitMap(string filename, vector<Tile> tiles, int tile_size)
{
	CLSID pngClsid;

	GetEncoderClsid(L"image/png", &pngClsid);
	
	Bitmap* new_bitmap = new Bitmap(tile_size, tiles.size() * tile_size);

	Color colour;

	for (int i = 0; i < tiles.size(); i++)
	{
		for (int x = 0; x < tile_size; x++)
		{
			for (int y = 0; y < tile_size; y++)
			{
				colour = tiles[i].tile[x][y].colour;
				new_bitmap->SetPixel(x, y + tile_size*i, colour);
			}
		}
	}

	Status stat;
	stat = new_bitmap->Save(GetFileName(filename, ".png"), &pngClsid, NULL);

	delete new_bitmap;

}

// Saves new_map to a .png
void SaveMap(string filename, vector<vector<int>> new_map, int tile_size)
{
	Bitmap* tile_map = new Bitmap(GetFileName(filename + "_tiles", ".png"));
	
	cout << tile_map->GetWidth() << "x" << tile_map->GetHeight() << endl;

	CLSID pngClsid;

	GetEncoderClsid(L"image/png", &pngClsid);

	int size = 0;

	// Chooses the larger between width and height and makes the output image square
	if (new_map[0].size() * tile_size > new_map.size() * tile_size)
	{
		size = new_map[0].size() * tile_size;
	}
	else
	{
		size = new_map.size() * tile_size;
	}

	Bitmap* new_bitmap = new Bitmap(size, size);

	cout << new_bitmap->GetWidth() << "x" << new_bitmap->GetHeight() << endl;

	Color colour;

	// Writes so the bitmap
	for (int y = 0; y < new_map.size(); y++)
	{
		for (int x = 0; x < new_map[0].size(); x++)
		{
			for (int i = 0; i < tile_size; i++)
			{
				for (int j = 0; j < tile_size; j++)
				{
					tile_map->GetPixel(j, i + tile_size* new_map[y][x], &colour);

					new_bitmap->SetPixel(x * tile_size + j, y * tile_size + i, colour);
				}
			}
		}
	}

	// Saves the bitmap
	Status stat;
	stat = new_bitmap->Save(GetFileName("map", ".png"), &pngClsid, NULL);

	delete new_bitmap;

}

// Gets a tile form the image, at a specific segment
Tile GetTile(Bitmap* image, int tile_size, int start_x, int start_y, int tile_list_size)
{
	Tile temp_tile;
	vector<Pixel> temp_row;

	Color temp_colour;

	for (int x = 0; x < tile_size; x++)
	{
		temp_row.clear();
		for (int y = 0; y < tile_size; y++)
		{
			image->GetPixel(start_x + x, start_y + y, &temp_colour);

			Pixel temp_pixel;

			temp_pixel.SetColour(temp_colour);

			temp_row.push_back(temp_pixel);
		}
		temp_tile.tile.push_back(temp_row);
	}

	temp_tile.id = tile_list_size;

	return temp_tile;
}

// Checks the pixels of two tile against each other, if there's any difference --> returns true
bool CheckTile(Tile tile_1, Tile tile_2, int tile_size)
{
	for (int x = 0; x < tile_size; x++)
	{
		for (int y = 0; y < tile_size; y++)
		{
			if (tile_1.tile[y][x].r != tile_2.tile[y][x].r ||
				tile_1.tile[y][x].g != tile_2.tile[y][x].g ||
				tile_1.tile[y][x].b != tile_2.tile[y][x].b)
			{
				return true;
			}
		}
	}

	return false;
}

// Compares two tiles, returns false if they are different
bool NewTile(vector<Tile> tile_list, Tile current_tile, int tile_size)
{
	bool check = false;

	for each (Tile tile in tile_list)
	{
		check = CheckTile(tile, current_tile, tile_size);
		if (check == false)
		{
			return false;
		}

	}

	return check;
}

// Looks for the current_tile in tile_list, if it's found returns its ID, else returns -1
int GetTileID(vector<Tile> tile_list, Tile current_tile)
{
	for each (Tile tile in tile_list)
	{
		if (CheckTile(tile, current_tile, current_tile.tile.size()) == false)
		{
			return tile.id;
		}
	}

	return -1;
}

//Initialises the grid at the correct size and with all the node set to tranversable
InitOutput InitGrid()
{
	InitOutput output;
	
	vector<int> v;

	Bitmap* bitmap = new Bitmap(L"route1.bmp");

	int width = bitmap->GetWidth();
	int height = bitmap->GetHeight();

	int tile_size = 16;

	Color pixel_color;

	Tile current_tile;

	for (int y = 0; y < height; y += tile_size)
	{
		v.clear();
		
		for (int x = 0; x < width; x += tile_size)
		{
			current_tile = GetTile(bitmap, tile_size, x, y, output.tile_list.size());

			if (NewTile(output.tile_list, current_tile, tile_size) || output.tile_list.size() == 0)
			{
				output.tile_list.push_back(current_tile);

			}
			v.push_back(GetTileID(output.tile_list, current_tile));
		}

		output.map.push_back(v);

		cout << (float)(width*y) / (float)(width*height) * 100 << "%" << endl;
	}


	delete bitmap;

	return output;
}

//Initialises the grid at the correct size and with all the node set to tranversable
InitOutput InitGrid(string filename)
{
	InitOutput output;

	vector<int> v;

	ifstream file(filename + ".csv");
	string line;
	
	//Tile current_tile;

	int total = 0;
	int current = 0;

	while(getline(file, line))
	{
		v.clear();

		stringstream line_stream(line);
		string cell;

		total = 0;
		while(getline(line_stream, cell, ','))
		{
			v.push_back(stoi(cell));
			total++;
		}

		output.map.push_back(v);

		current++;
	}


	//vector<vector<string>> temp_grid;
	vector<int> temp_line;

	/*ifstream file_tiles(filename + "_tiles.csv");

	current = 0;

	while (getline(file_tiles, line))
	{
		v.clear();

		stringstream line_stream(line);
		string cell;

		temp_line.clear();

		while (getline(line_stream, cell, ','))
		{
			temp_line.push_back(stoi(cell));
		}

		Tile temp_tile;
		temp_tile.id = temp_line[0];
		temp_tile.colour = Color(255, temp_line[1], temp_line[2], temp_line[3]);
		output.tile_list.push_back(temp_tile);

		current++;
	}*/

	Bitmap* bitmap = new Bitmap(GetFileName(filename + "_tiles", ".png"));

	int width = bitmap->GetWidth();
	int height = bitmap->GetHeight();

	//cout << width << "x" << height << endl;


	Tile current_tile;
	vector<Pixel> temp;

	Color colour;

	for (int h = 0; h < height/width; h++)
	{
		
		current_tile.id = h;
		//current_tile.

		for (int y = 0; y < width; y++)
		{
			for (int x = 0; x < width; x++)
			{
				Pixel temp_pixel;
				bitmap->GetPixel(x, y + width * h, &colour);
				temp_pixel.SetColour(colour);
				temp.push_back(temp_pixel);
			}
			current_tile.tile.push_back(temp);
			temp.clear();
		}
		output.tile_list.push_back(current_tile);
		current_tile.tile.clear();

	}

	delete bitmap;

	cout << "Reading " << filename << ".csv: " << 100 << "%" << endl;

	//PrintBitMapToScreen(output.map[0].size(), output.map.size(), 200, 50, output.map, output.tile_list, 2);

	

	return output;
}

// Converts char* to wchar_t
// From: https://stackoverflow.com/questions/8032080/how-to-convert-char-to-wchar-t?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
const wchar_t *GetWC(const char *c)
{
	const size_t cSize = strlen(c) + 1;
	wchar_t* wc = new wchar_t[cSize];
	//mbstowcs_s( cSize, wc, c);

	return wc;
}

// Opens an image called 'name' reads in the data and converts it to a .csv file
bool ReadMapToCSV(string name, int tile_size)
{
	InitOutput output;
	vector<int> v;
	
	Bitmap* bitmap = new Bitmap(GetFileName(name, ".bmp"));

	int width = bitmap->GetWidth();
	int height = bitmap->GetHeight();

	if (width == 0 && height == 0)
	{
		std::cout << "ERROR: FILE NOT FOUND" << std::endl;
		return false;
	}

	cout << width << "x" << height << endl;

	Tile current_tile;

	for (int y = 0; y < height; y += tile_size)
	{
		int perc = (float)(width*y) / (float)(width*height) * 100;
		cout << "Reading in bitmap: " << perc << "%" << endl;
		
		v.clear();

		for (int x = 0; x < width; x += tile_size)
		{
			current_tile = GetTile(bitmap, tile_size, x, y, output.tile_list.size());

			if (NewTile(output.tile_list, current_tile, tile_size) || output.tile_list.size() == 0)
			{
				output.tile_list.push_back(current_tile);
			}
			v.push_back(GetTileID(output.tile_list, current_tile));
		}

		output.map.push_back(v);

	}

	cout << "Reading in bitmap: " << 100 << "%" << endl;
	delete bitmap;

	ofstream fs;

	string filename = name;
	filename += ".csv";

	fs.open(filename);

	for (int x = 0; x < output.map.size(); x++)
	{
		for (int y = 0; y < output.map[0].size(); y++)
		{
			fs << output.map[x][y] << ",";
		}
		fs << endl;
	}

	cout << "Writing to " << filename << ": " << 100 << "%" << endl;

	fs.close();

	SaveTileBitMap(name + "_tiles", output.tile_list, tile_size);
	
	return true;
}

// Checks a tile against a specific portion of the map, based on the d_matrix
bool CheckMapSegment(tile_holder_3* th3, vector<vector<int>> map, int x, int y, int start)
{
	int Ax = 0, Ay = 0;
	int num = 0, num2 = 0;


	for (int d_y = start; d_y < 3; d_y++)
	{
		Ay = 2 - d_y;
		
		for (int d_x = start; d_x < 3; d_x++)
		{
			if (d_matrix[d_y][d_x] == 1)
			{				
				Ax = 2 - d_x;

				num = map[y - Ay][x - Ax];
				num2 = map[y][x];

				if (th3->neighbour[d_y][d_x] != map[y - Ay][x - Ax])
				{
					return false;
				}
			}
		}
	}

	return true;
}

// Calculates frequency distribution and performs map generation
void UpLeftPlus(InitOutput output, string filename, int tile_size, int splits)
{
	srand(time(NULL));
	
	HWND consoleWindow = GetConsoleWindow();

	SetWindowPos(consoleWindow, 0, 500, 300, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	int start = 1;

	vector<vector<int>> map = output.map;
	vector<Tile> tile_list = output.tile_list;

	int WIDTH = map[0].size();
	int HEIGHT = map.size();
	int number_of_tiles = tile_list.size();

	vector <list<tile_holder_3*>> freq_map_big;

	bool found = false;

	int split_height = HEIGHT / splits;
	int split_remainder = HEIGHT - (splits * split_height);

	// Totals the number of tiles, depending on the dependancy matrix (d_matrix)
	for (int s = 0; s < splits; s++)
	{
		list<tile_holder_3*> temp_freq_map;

		for (int y = s * split_height; y < (s+1) * split_height; y++)
		{
			int perc = (float)(WIDTH*y) / (float)(WIDTH*HEIGHT) * 100;
			cout << "Calculating frequency distribution: " << perc << "%" << endl;
			
			// Doesnt include the first two rows
			if (y > 1 || s > 0)
			{
				// Doesnt inlcude the first two columns
				for (int x = 2; x < WIDTH; x++)
				{
					found = false;

					// Check sto see if the current tile has been found before
					for each (tile_holder_3* th3 in temp_freq_map)
					{
						found = CheckMapSegment(th3, map, x, y, start);
						
						// If its been found
						if (found == true)
						{			
							// Count it
							th3->CountTile(map[y][x]);
							break;
						}
					}

					// Current tile s a new tile
					if (found == false)
					{
						tile_holder_3* temp = new tile_holder_3;

						temp->Init(number_of_tiles);

						// Work out the neighbours
						for (int d_y = 1; d_y < 3; d_y++)
						{
							for (int d_x = 1; d_x < 3; d_x++)
							{
								if (d_matrix[d_y][d_x] == 1)
								{
									temp->neighbour[d_y][d_x] = map[y - (2 - d_y)][x - (2 - d_x)];
								}
							}
						}

						// Add the new tile
						temp->neighbours_count[map[y][x]]++;
						temp->total_count++;
						temp_freq_map.push_back(temp);
					}				
				}
			}
		}

		freq_map_big.push_back(temp_freq_map);
	}

 	cout << "Calculating frequency distribution: " << 100 << "%" << endl;

	vector<vector<int>> new_map;
	vector<int> v;

	// Sets up blank map
	for(int x = 0; x < HEIGHT; x++)
	{
		v.clear();
	
		for (int y = 0; y < WIDTH; y++)
		{
			v.push_back(0);
			
		}
		new_map.push_back(v);
	}
	for (int x = 0; x < 2; x++)
	{
		v.clear();

		for (int y = 0; y < WIDTH; y++)
		{
			new_map [x][y] = map[x][y];

		}
	}
	for (int x = 0; x < HEIGHT; x++)
	{
		v.clear();

		for (int y = 0; y < 2; y++)
		{
			new_map[x][y] = map[x][y];

		}
	}

	float num = 0.0f;

	bool tile_found = false;

	bool unseen_state = false;

	int look_ahead = 3;
	int s = 0;
	int retrys1 = 0;
	int retrys2 = 0;
	int current_max_x = 2;
	int current_max_y = 2;

	int step_amount = 5;

	// Initialise the random number generator
	std::random_device rd;
	std::mt19937 gen(rd());

	int perc = 0;

	// Creates the random generator for numbers between 0 and one less than the number of posible spawn location
	std::uniform_real_distribution<> distribution(0.0f, 100.0f);

	for each (list<tile_holder_3*> freq_map in freq_map_big)
	{
		for (int y = s * (HEIGHT / splits); y < (s + 1) * (HEIGHT / splits); y++)
		{
			perc = ((float)y / (float)HEIGHT)*100.0f;
			std::cout << perc << "%" << std::endl;
			current_max_y = y;
			if (y > 1)
			{
				current_max_x = 2;
				for (int x = 2; x < WIDTH; x++)
				{
					if (x > 1)
					{	
						num = distribution(gen);
						
						float p = 0.0f;						
						tile_found = false;						
						bool map_seg = false;
						
						// Check the current ttile combination
						for each (tile_holder_3* nh in freq_map)
						{
							// Checks to see if its a possible combination
							if (CheckMapSegment(nh, new_map, x, y, start))
							{
								unseen_state = false;
								
								// Gets a tile
								for (int i = 0; i < number_of_tiles; i++)
								{
									// Gets the probabilty for the current itle
									float k = ((float)nh->neighbours_count[i] / nh->total_count) * 100;
									p += k;
						
									// The new tile is added the the map
									if (num < p)
									{
										new_map[y][x] = i;
										tile_found = true;
										break;
									}
								}
							}
							else
							{
								// There is a problem with the neighbours	
								unseen_state = true;
							}

							if (tile_found == true)
							{
								break;
							}
						}
						
						// Problem resolution
						if (unseen_state == true)
						{
							//Goes back step_amount number of tiles
							current_max_x = x;
							retrys1++;
							x -= step_amount;
							if (x <= 2)
							{
								x = 2;
							}
						}
						
						// The unseen state has been passed
						if (x > current_max_x && y > current_max_y)
						{
							retrys1 = 0;
							retrys2 = 0;
						}
						
						// Has tried going back 10 times, go diagonally up and back
						if (retrys1 == 10)
						{
							retrys1 = 0;
							retrys2++;

							x--;
							y--;
							current_max_x = x;
							current_max_y = y;
							if (y < 2)
							{
								y = 2;
							}
							if (x < 2)
							{
								x = 2;
							}
						}	
					}
				}
			}
		}
		s++;
	}

	// Saves the map
	SaveMap(filename, new_map, tile_size);

}

//Prompts the user to enter a int, loops until the input is a number, and its between min and max
int GetNum(int min, int max)
{
	int tempNum = 0;

	std::cin >> tempNum;
	while (cin.fail() || tempNum < min || tempNum > max) //if the users enters anything other than a number
	{
		std::cin.clear();
		std::cin.ignore(1000, '\n');

		std::cout << endl;

		std::cout << "Please input a valid integer >= to " << min << " and <= to " << max << ")" << endl << endl;
		std::cin >> tempNum;
	}

	return tempNum;
}

bool CheckImage(string name)
{
	Bitmap* bitmap = new Bitmap(GetFileName(name, ".bmp"));

	int width = bitmap->GetWidth();
	int height = bitmap->GetHeight();

	delete bitmap;

	if (width == 0 && height == 0)
	{
		std::cout << "ERROR: FILE NOT FOUND" << std::endl;
		return false;
	}
	
	return true;
}

int main()
{
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;

	// Initialize GDI+.
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	
	string name;
	
	int tile_size = 16;
	int splits = 1;

	cout << "Please enter the bitmap's filename (exclude '.bmp'): ";
	cin >> name;
	
	while (CheckImage(name) == false)
	{
		cout << "Please enter the bitmap's filename (exclude '.bmp'): ";
		cin >> name;
	}

	cout << "How wide is each tile?: ";
	cin >> tile_size;

	cout << "Do you want to read in an image? (1 = yes, 0 = no): ";
	int read_in = GetNum(0, 1);

	if (read_in == 1)
	{
		// Reads in a image
		while (!ReadMapToCSV(name, tile_size))
		{
			cout << "Please enter the correct bitmap's filename (exclude '.bmp'): ";
			cin >> name;

			cout << "How wide is each tile?: ";
			tile_size = GetNum(1, 50);

			cout << "How many splits?: ";
			splits = GetNum(1, 50);
		}
	}
	

	// Initialises the various maps
	InitOutput op = InitGrid(name);

	// Markov Chains
	UpLeftPlus(op, name, tile_size, splits);

	Gdiplus::GdiplusShutdown(gdiplusToken);

	return 0;
}