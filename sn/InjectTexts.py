import json
import glob
from pathlib import Path
import os

Filenames = [
	"purorogu",
	"1/day1",
	"1/day2",
	"1/day3",
	"1/day4",
	"2/day1",
	"2/day2",
	"2/day3",
	"2/day4",
	"3/day1",
	"3/day2",
	"3/day3",
	"3/day4",
	"4/day1",
	"4/day2",
	"4/day3",
	"4/day4",
	"5/day1",
	"5/day2",
	"5/day3",
	"5/day4",
	"6/day1",
	"6/day2",
	"6/day3",
	"6/day4",
	"if",
	"if123",
	"if456",
	"omake"
]

os.makedirs("Patched", exist_ok=True)

for i in range(len(Filenames)):
	try:
		file = open(f"Patches/{Filenames[i]}.json", "r", encoding="UTF-8")
	except:
		continue
	print(Filenames[i])
	patch = json.load(file)
	file.close()
	
	file = open(f"jsons/{Filenames[i]}.json", "r", encoding="UTF-8")
	DUMP = json.load(file)
	file.close()
	
	y = 0
	for x in range(len(DUMP["COMMANDS"])):
		if (DUMP["COMMANDS"][x]["LABEL"] == patch[y]["LABEL"]):
			DUMP["COMMANDS"][x] = patch[y]
			y += 1
			if (y == len(patch)):
				break
	
	assert(y == len(patch))

	os.makedirs(os.path.dirname(f"Patched/{Filenames[i]}.json"), exist_ok=True)
	new_file = open(f"Patched/{Filenames[i]}.json", "w", encoding="UTF-8")
	json.dump(DUMP, new_file, indent="\t", ensure_ascii=False)
	new_file.close()