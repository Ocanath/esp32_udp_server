This code is for turning an ESP32 into a UART to UDP bridge.

Any received UDP packets will be forwarded as UART TX dataframes with no modifications.
Any received PPP-byte stuffed UART dataframes will be unstuffed and sent over UDP.

Important settings (minimum for functionality):
    These settings can be accessed using the ESP32's UART0 in a command line-type interface.
        setport: sets the port our (single) udp socket binds to
        setTXoff: special setting for 'split streams'. It's a number added to the port the ESP32's socket is bound to, which we route outgoing UDP traffic to.
                    The intended use is PC's, where you want to split up the transmission and reception into multiple processes
        setssid: the SSID of the network the ESP32 will continuously try to connect to
        setpwd: the password of the ESP32 wifi network we continuously try to connect to
        setbaud: set the UART baud rate FOR THE UART USED IN THE FORWARDING, NOT THE COMMAND LINE


More Info:

    If you're in the IOT branches (relay board and switcher offshoot branches), there's more setup:

    1. Use the minimum necessary commands above for network functionality. I.e. set port, wifi ssid and password.
    1.1 SET A NAME BESIDES THE DEFAULT: setname "EXAMPLE NAME"
    2. use chat.py, bind to a port on the same network/subnet. Set your name to NOTHING. Then, you can type commands from the udp command section, such as lightson lightsoff. 
    3. If your board has a name, you can use that name as a prefix for most commands in the udp command section