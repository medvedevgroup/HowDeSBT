# HowDeSBT
Sequence Bloom Tree, supporting determined/how split filters

# Dependencies

* Jellyfish (Version 2.2.0 or later)
* SDSL-lite (Has to be cloned from github.com/simongog/sdsl-lite after
April 2017; earlier versions had a problem with RRR in certain configurations)
* CRoaring (https://github.com/RoaringBitmap/CRoaring)
* The tutorial makes use of ntCard (https://github.com/bcgsc/ntCard)

# Installation

To install subutan from the source:  
1a. Download the latest version of subutan using Github  
```bash  
     git clone https://github.com/medvedevgroup/HowDeSBT  
```  
1b. If you have installed the dependencies somewhere other than
<code>${HOME}</code>, you need to modify the Makefile. Specifically, in
both the <code>CXXFLAGS</code> and <code>LDFLAGS</code> definitions
<code>$${HOME}</code> should be changed to your install path.
1c. Jellyfish installation requires an extra step for its include
directory. After you have installed Jellyfish, do
```bash  
    cd ${HOME}/include
    ls | grep jellyfish
```
You should see something like
```bash  
    jellyfish-2.2.6
```
where 2.2.6 is the version of Jellyfish you've installed. Then make a
symbolic link named 'jellyfish' that points to the includes directory
for the version you've installed:
```bash  
    cd ${HOME}/include
    ln -s jellyfish-2.2.6/jellyfish jellyfish
```
2. Compile:  
```bash  
    cd HowDeSBT  
    make  
```
3. A usage tutorial can be found at
https://github.com/medvedevgroup/HowDeSBT/tree/master/tutorial
