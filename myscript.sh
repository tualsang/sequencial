#!/bin/bash
#SBATCH --partition=Centaurus
#SBATCH --time=01:00:00
#SBATCH --mem=64G

# Compile the executable
g++ -I$HOME/rapidjson/include -o level_client level_client.cpp -lcurl

# Run the program with different starting nodes and traversal depths
./level_client "Tom Hanks" 4
