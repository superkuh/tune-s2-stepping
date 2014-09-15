tune-s2-stepping
================

The normal tune-s2 program is maintained by CrazyCat over at https://bitbucket.org/CrazyCat/tune-s2 and it does many things.
This is a gutted tune-s2 exclusively for sending satellite motor positioning Diseqc commands through the DVB interface.

What is great about tune-s2 is that it works with diseqc switches. That way one DVB card can send motor commands to many
motors. I have not altered the front-end setup or switch aspects of tune-s2 at all and they can be used normally. The only
difference is that my copy will never try to tune the DVB card/LNB.

Usage is pretty simple. The only differences being two new cli switches.

<pre>-step-east 
-step-west</pre>

They each take any value from 0 to 10 like, 

<pre>./tune-s2 12224 V 20000 -2 -step-west 0 -committed 1</pre>

This would cause the satellite dish motor on port 1 of the Diseqc switch to step 1 position counter-clockwise.

The stepping argument values 0 through 10 are mapped on a fairly arbitrary set of actual steps,

<pre>0->1
1->2
2->3
3->4
4->5
5->10
6->20
7->30
8->40
9->50
10->100</pre>

And yeah, for now you have to include the "12224 V 20000" stuff even though software never uses it. Here's the usage
for the original program:

<pre>usage: tune-s2 12224 V 20000 [options]
	-adapter N     : use given adapter (default 0)
	-frontend N    : use given frontend (default 0)
	-2             : use 22khz tone
	-committed N   : use DiSEqC COMMITTED switch position N (1-4)
	-uncommitted N : use DiSEqC uncommitted switch position N (1-4)
	-servo N       : servo delay in milliseconds (20-1000, default 20)
	-gotox NN      : Drive Motor to Satellite Position NN (0-99)
	-usals N.N     : orbital position
	-long N.N      : site long
	-lat N.N       : site lat
	-lnb lnb-type  : STANDARD, UNIVERSAL, DBS, CBAND or 
	-system        : System DVB-S or DVB-S2
	-modulation    : modulation BPSK QPSK 8PSK
	-fec           : fec 1/2, 2/3, 3/4, 3/5, 4/5, 5/6, 6/7, 8/9, 9/10, AUTO
	-rolloff       : rolloff 35=0.35 25=0.25 20=0.20 0=UNKNOWN
	-inversion N   : spectral inversion (OFF / ON / AUTO [default])
	-pilot N	   : pilot (OFF / ON / AUTO [default])
	-mis N   	   : MIS #
	-help          : help</pre>
