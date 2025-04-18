#!/bin/bash
#
#
# install script for PiDP-1
#
#PATH=/usr/sbin:/usr/bin:/sbin:/bin


# check this script is NOT run as root
if [ "$(whoami)" = "root" ]; then
    echo script must NOT be run as root
    exit 1
fi

if [ ! -d "/opt/pidp1" ]; then
    echo clone git repo into /opt/
    exit 1
fi

echo
echo
echo PiDP-1 install script
echo ======================
echo
echo The script can be re-run at any time to change things. Re-running the install
echo script and answering \'n\' to questions will leave those things unchanged.
echo You can recompile from source, but it is easier to just install the precompiled
echo binaries. 
echo
echo Too Long, Didn\'t Read?
echo Just say Yes to everything.
echo
echo

usr=$(whoami)
usrgroup=$(id -g -n)
cd /opt/pidp1

# give pidp1 user owner after the sudo git clone command
# =============================================================================

while true; do
    echo
    read -p "Set owner of PiDP-1 directory? " yn
    case $yn in
        [Yy]* )
            # make sure that the directory does not have root ownership
            sudo chown -R $usr:$usrgroup /opt/pidp1
	    break
	    ;;
        [Nn]* ) 
            echo Left ownership of PiDP directory unchanged
	    break
            ;;
        * ) echo "Please answer yes or no.";;
    esac
done


# pull in git submodules (for pdp1 sim itself, and the p7sim Type 30 display)
# =============================================================================

while true; do
    echo
    read -p "Update git submodules (required for first install)? " yn
    case $yn in
        [Yy]* )
		git submodule init
		git submodule update --remote --force
		break
		;;
        [Nn]* ) 
            echo Skipped updating submodules from github
	    break
            ;;
        * ) echo "Please answer yes or no.";;
    esac
done


# Install required dependencies
# =============================================================================
while true; do
    echo
    read -p "Install the required dependencies? " prxn
    case $prxn in
        [Yy]* ) 
            sudo apt update
            #Install SDL2, optionally used for PDP-11 graphics terminal emulation
            sudo apt install -y libsdl2-dev
	    #Install SDL2-image. Strictly speaking, only needed for virtual panel, not PiDP-1
	    sudo apt install -y libsdl2-image-dev
            #Install ncat
            sudo apt install -y ncat
            #Install readline, used for command-line editing in simh
            #sudo apt install -y libreadline-dev
            # Install screen
            sudo apt install -y screen

	    # not needed for Pi OS but for generic Linux
	    sudo apt install lxterminal

	    break
	    ;;
        [Nn]* ) 
            echo Skipped install of dependencies - OK if you installed them already
            break
	    ;;
        * ) echo "Please answer Y or N.";;
    esac
done


# make required binaries
# =============================================================================

while true; do
    echo
    read -p "Make required PiDP-1 binaries? " yn
    case $yn in
        [Yy]* )
		make -C /opt/pidp1/src/blincolnlights/pinctrl 	# pinctrl functions
		make -C /opt/pidp1/src/blincolnlights/panel_pidp1 	# panel driver
		make -C /opt/pidp1/src/blincolnlights/pdp1 	# simulator
		make -C /opt/pidp1/src/p7sim			# returns sense switches
		make -C /opt/pidp1/src/scanpf 			# returns sense switches
		make -C /opt/pidp1/src/blincolnlights/tapevis	# returns sense switches
		make -C /opt/pidp1/src//pidp1_test 	# hardware test program
		
		# this makes the virtual pdp-1 panel, used if no PiDP-1 hardware is attached:
		make -C /opt/pidp1/src/blincolnlights/vpanel_pdp1 	# panel driver
            
		echo Setting required access privileges to pidp1 simulator
		# make sure pidp1 panel driver has the right privileges
            	# to access GPIO with root privileges:
            	sudo chmod +s /opt/pidp1/src/blincolnlights/panel_pidp1
            	# to run as a RT thread:
            	sudo setcap cap_sys_nice+ep /opt/pidp1/src/blincolnlights/panel_pidp1/panel_pidp1
	    	echo Done.
		break
		;;
        [Nn]* ) 
            echo Did not compile PiDP-1 programs.
	    break
            ;;
        * ) echo "Please answer yes or no.";;
    esac
done


# Use virtual panel or PiDP hardware panel
# =============================================================================

while true; do
    echo
    read -p "Use PiDP hardware front panel (Y) or on-screen virtual panel (V)? " yn
    case $yn in
        [Yy]* )
	    echo Activated PiDP hardware front panel
	    ln -sfn /opt/pidp1/src/blincolnlights/panel_pidp1/panel_pidp1 /opt/pidp1/bin/panel_pidp1
	    break
            ;;
        [Vv]* ) 
            echo Activated virtual front panel - PiDP hardware deactivated
	    ln -sfn /opt/pidp1/src/blincolnlights/vpanel_pdp1/panel_pdp1 /opt/pidp1/bin/panel_pidp1
	    break
            ;;
        * ) echo "Please answer yes or no.";;
    esac
done


# Install pidp1 commands
# =============================================================================
while true; do
    echo
    read -p "Install PiDP-1 commands into OS? " prxn
    case $prxn in
        [Yy]* ) 
            # put pdp1 command into /usr/local
            sudo ln -f -s /opt/pidp1/bin/pdp1.sh /usr/local/bin/pdp1
            # put pdp1control script into /usr/local
            sudo ln -f -s /opt/pidp1/bin/pdp1control.sh /usr/local/bin/pdp1control
	    break
	    ;;
        [Nn]* ) 
            echo Skipped software install
            break
	    ;;
        * ) echo "Please answer Y or N.";;
    esac
done


# Install autostart at boot
# =============================================================================
if [ "$ARCH" = "amd64" ]; then
	echo skipping autostart, because this is not a Raspberry Pi
	echo start manually by typing 
	echo pdp1control start x
	echo ...where x is the tape number normally set on the front panel.
	echo 
	echo "But that is all in the manual..."
	echo
else
	while true; do
	    echo
	    echo 
		read -p "Autostart the PDP-1 using the GUI(Y), or using .profile for (H)eadless Pis, or (N)ot at all?" yhn
		case $yhn in
		      [Yy]* ) 
			mkdir -p ~/.config/autostart
			cp /opt/pidp1/install/pdp1startup.desktop ~/.config/autostart
			echo
			echo Autostart via .desktop file for GUI setup
			break
			;;
		      [Hh]* ) 
			# add pdp11 to the end of pi's .profile to let a new login 
			# grab the terminal automatically
			#   first, make backup .foo copy...
			test ! -f /home/$usr/profile.foo && cp -p /home/$usr/.profile /home/$usr/profile.foo
			#   add the line to .profile if not there yet
			if grep -xq "pdp11 # autostart" /home/$usr/.profile
			then
			    echo .profile already contains pdp11 for autostart, OK.
			else
			    sed -e "\$apdp11 # autostart" -i /home/$usr/.profile
			fi
			echo
			echo autostart via .profile for headless use without GUI
			break
			;;
		      [Nn]* ) 
			echo No autostart
			break
			;;
		      * ) echo "Please answer Y, H or N.";;
	    esac
	done	
fi

# 20241126 Add desktop icons etc
# =============================================================================
while true; do
    echo
    read -p "Add desktop icons and desktop settings? " prxn
    case $prxn in
        [Yy]* ) 
            cp /opt/pidp1/install/tty.desktop /home/$usr/Desktop/
            cp /opt/pidp1/install/pdp1control.desktop /home/$usr/Desktop/
            cp /opt/pidp1/install/type30.desktop /home/$usr/Desktop/
            cp /opt/pidp1/install/ptr.desktop /home/$usr/Desktop/
            cp /opt/pidp1/install/ptp.desktop /home/$usr/Desktop/

            #make pcmanf run on double click, change its config file
            config_file="/home/$usr/.config/libfm/libfm.conf"
            # Create the directory if it doesn't exist
            mkdir -p "$(dirname "$config_file")"
            # Add or update the quick_exec setting
            if grep -q "^\s*quick_exec=" "$config_file" 2>/dev/null; then
                echo ...Update existing setting...
                sed -i 's/^\s*quick_exec=.*/quick_exec=1/' "$config_file"
            else
                echo ...Adding the config file, it does not exist yet
                echo -e "[config]\nquick_exec=1" >> "$config_file"
            fi
        
            # wallpaper
            pcmanfm --set-wallpaper /opt/pidp1/install/wallpaper.png --wallpaper-mode=fit

            #echo
            #echo "Installing Teletype font..."
            #echo
            #mkdir ~/.fonts
            #    cp /opt/pidp1/install/TTY33MAlc-Book.ttf ~/.fonts/
            #fc-cache -v -f


            echo "Desktop updated."
            break
	    ;;

        [Nn]* ) 
            echo Skipped. You can do it later by re-running this install script.
            break
	    ;;
        * ) echo "Please answer Y or N.";;
    esac
done


echo
echo Done. Please do a sudo reboot and the front panel will come to life.
echo Rerun this script if you want to do any install modifications.

