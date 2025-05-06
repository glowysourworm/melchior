# melchior
Project code name "melchior" is an attempt at an integrated IDE for SNES ROM development based on pvsneslib (source code) by alekmaul.

# background
The availability of compiler tools was maintained by alekmaul (and others). There are several examples of compiler automation out there
for getting very simple ROM's done. However, there is a lack of a completed IDE for simplifying the whole process.. which amazes me
because there are plenty of hobbyists. 

# requirements
This project will begin with an integrated compiler using the source from pvsneslib. Compilation of example projects is the first 
goal of this project. After that, rigging up the compiler tools and reading through docs would be in order. A full debugger requires
completing this (training). And, an emulator backend will become another part of the project.. (to be continued)

# collaboration!
If you're willing work on the project, just send me an email about how (best) to go about putting pieces together. There are plenty
of little things to consider:  how to integrate a proper image quantizer (this is a common error); how to package different pieces
of the SNES requirements (understanding of LOMEM / HIMEM / ...); or how to handle debugging with Snes9x (would be an important one).
