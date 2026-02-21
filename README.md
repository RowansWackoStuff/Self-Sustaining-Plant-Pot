# SelfSustainingPlantPot
This project is an attempt at making a self watering plant pot that can water plants overtime with custom settings

Goals:
- enough water to last a week
- supplied by battery power
- release water consistently
bonus goals:
- controlled through webserver
- setting for watering seeds overtime

I wanted to start with a pot that contains the soil and the electronic components which could have an external silo attached that could be filled with water. The pot diameter has to be at least 15cm and the height must be 15cm in order for proper root development. With this in mind a rough sketch can be made
<div align="center">
<img src="pictures/sketch.png" width="50%"/>
</div>

## Piping

First step I wanted to find out how the water pipe set up would be done. in order to do this i started by testing different pipe hole diameters, amount of holes and pipe length until I found the desired presure 

<div align="center">
<img src="pictures/Pipetest.jpg" width="50%"/>
</div>

after some trial and error the amount of holes and diameter was determined and made into a ring


<div align="center">
<img src="pictures/ring.PNG " width="50%"/>
</div>

Adding an extension pipe from the pump to the ring reduces the amount of pressure in the piping so I assumed I would need to alter the rings hole's size but the presure was sustainable enough to travel te distance and exit the holes perfectly.
(This one was a bit over the top)
<div align="center">
<img src="pictures/ringpipe.PNG" width="50%"/>
</div>

## electronics
The electronics involved would have to be a power supply, a microcontroller and a small water pump. for the microcontroller I chose the esp32-c3 supermini for its size and web hosting capabilities. 

The power supply for the esp32 needed a minimum of 3.3V and had to last a long period of time. With the sumpermini consuming 0.4ma per hour atmost in deepsleep and the estimated time for a lettuce to mature being at most 11 weeks (77 days, 1,848 hours) then the power supply needs 0.004 * 1,848 = 7392mah

Using a configuration of 3 AA batteries in series the collective voltage is 4.5V and has a current supply of 8,100mah satisfying the needs of the system

Starting out I used a transistor to deliver 5V to the pump through the board. This set up didnt work due to the poor flow of current through the resistor limiting the output of the water pump.
<div align="center">
<img src="pictures/breadboard.jpg" width="50%"/>
</div>

In order to fix this issue I swapped the transistor for a relay with an external power supply. This solution also solved the voltage drop the esp was suffering from which caused it to reset everytime the motor turned on.
<div align="center">
<img src="pictures/pumpcircuit.PNG" width="50%"/>
</div>

## programming

In order to interact with the system I used the ESPAsyncWebServer library which allowed for the esp to run an asyncronous webserver off its wifi. This wifi can be connected with through any device in order to open a webpage containing the user interface of the system.

This user interface consists of a water setting for default watering a plant daily with the same amount aswell as a seed setting the takes a minimum and maximum amount of water as well as the amount of days to water in order grow seeds. The seeds are watered exponentially over the days as in the early stage of development the seeds need a small amount of water and in the mature stages require a drastically larger amount of water.

## webserver
## deepsleep





## Testing pot

## math for pot
A small lettuce needs roughly less than 1 cup of water a day for growth. With this in mind the height of the reserviour can be calculated.

1 cup = 250ml
7 cups = 1750ml ~ 2L ~ 2000ml
2L = 2000cm^3

cylinder volume = pi*r^2*h

2000cm = pi * 7.5^2 * h
11.32cm = h

with this the height of the reserviour will be rounded 10cm 

## clip design

// INSERT ROTATION LINK MODEL HERE


<div>
<img src="pictures/testpot.jpg" width="50%"/><img src="pictures/testdog.jpg" width="50%"/>
</div>

## FINISHED

<div align="center">
<img src="pictures/parts.jpg" width="50%"/>
</div>

<div align="center">
<img src="pictures/final.jpg" width="50%"/>
</div>

# Conclusion

Goals:
- enough water to last a week ✅
- supplied by battery power ✅
- release water consistently ✅
bonus goals:
- controlled through webserver ✅
- setting for watering seeds overtime ✅


# mistakes/problems on the way
- solidworks saving files in order incorrectly (couldnt re print pot)
- the exteranl pipe leaks
- battery box clips seperated (need to learn better techniques)

# Improvements for Next time:
- reduce locking seal length (frictions very strong)
- get external clock from the esp as the esp couldnt track how longs its slept for
- improve seals for water pipes
- incorporate pipe into waterpot for better astetic
- create a custom pcb to reduce soldering errors
- 

THANKS FOR READING!
