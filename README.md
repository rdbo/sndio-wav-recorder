# WAV Recorder
WAV file recorder for sndio

## Usage
Compile the program:
```
make
```

Enable audio recording:
```
sysctl kern.audio.record=1
```

Run the program:
```
./main
```

The output file will be in 'test.wav'
