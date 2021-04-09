notesChars = ['A', 'B', 'C', 'D', 'E', 'F', 'G']
pauseChar = 'P'

def window(iterable, size=2):
	i = iter(iterable)
	win = []
	for e in range(0, size):
		win.append(next(i))
	yield win
	for e in i:
		win = win[1:] + [e]
		yield win

def parseMelody(melodyString, bpm=120, transposeBySemitones=0):
	# parse string to python list
	tokenizedNotes = melodyString.split(' ')
	operations = []
	for token, nextToken in window(tokenizedNotes + [None], 2):
		if token.startswith(pauseChar):
			# Token is a pause operation, use frequency 0
			operations.append([0, getDurationInMs(bpm, token[1:])])
		elif token.startswith(tuple(notesChars)):
			# Token is a note; next token will be duration of this note
			frequency = getFrequency(token, transposeBySemitones)
			duration = getDurationInMs(bpm, nextToken)
			operations.append([frequency, duration])
		else:
			continue
	# turn list into C-style array string
	arrayString = generateArrayString(operations)
	return arrayString

def getFrequency(note, transposeBy=0, A4=440):
	# example note: A#5, meaning: 5th octave A sharp
	notes = ['A', 'A#', 'B', 'C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#']
	octave = int(note[2]) if len(note) == 3 else int(note[1])
	keyNumber = notes.index(note[0:-1])
	if keyNumber < 3:
		keyNumber = keyNumber + 12 + ((octave - 1) * 12) + 1
	else:
		keyNumber = keyNumber + ((octave - 1) * 12) + 1
	keyNumber += transposeBy
	return int(A4 * 2 ** ((keyNumber - 49) / 12))

def getDurationInMs(bpm, duration):
	return int((1000 * (60 * 4 / bpm)) / float(duration))

def generateArrayString(melodyArray):
	# generate C-style array string from python list
	elements = []
	for element in melodyArray:
		elements.append("{" + str(element[0]) + "," + str(element[1]) + "}")
	return "{" + ','.join(elements) + "}"
