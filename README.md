# ShellularAutomata

<img src="shell.jpg" width="50%" height="50%">

An continuous CA engine building on the works of [Slackermanz](https://github.com/Slackermanz/VulkanAutomata).

## Build on Linux

### Debian-based distros
Install all dependencies with this command:
```
sudo apt install -y build-essential libglew-dev libglfw3-dev
```

The build with these commands once all the dependencies are met:
```
mkdir build && cd build
cmake ..
make -j
```

Then run with 
```
./ShellularAutomata
```