# Sofar_cnt
Control Sofar solar ME3000SP  battery invertor to work with Octopus Energy Agile tariff.
On this tariff the cost of energy changes  every half hour. This program downloads the next days prices and a wether forecast once a day and works out when to use the battery and when to charge the battery so as to minimize cost and help balance the load on the grid.
This program works off line requiring only one connection to the internet/day for those with unreliable internet connections.
This program was based on  Sofar2mqtt design c)Colin McGerty 2021 colin@mcgerty.co.uk https://github.com/cmcgerty/Sofar2mqtt Thanks for making it available

Still in development play at your own risk, but has running successfully for a few weeks now. 

# Hardware

Uses the ESP32 processor development board described here https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/tree/main. This includes a touch screen.  
A RS485 to UART adaptor is required to communicate with the Sofar invertor.

[missing image](min_boards.png "This is the minimum hardware required")

# Software
Platform Arduino
Board selected in Arduino is"Node32s" (Because I tried it and it worked)




