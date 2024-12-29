#!/bin/bash

user=`whoami`

echo "User: $user"

my_groups=`id -G -n`

echo "Groups: $my_groups"

function check_permissions(){

    local port=$1

    local permissions=`ls -l $port | cut -f1`

    owner=`ls -l $port | cut -d' ' -f3`
    echo "Owner: $owner"

    group=`ls -l $port | cut -d' ' -f4`
    echo "Group: $group"

    if [[ "$owner" == "$user" ]]; then
        echo "You are the owner of port $port. You should be able to change its permissions freely"
        echo "Run 'chmod u,g,o=rw $port' set read/write permissions for everyone"
        continue
    fi

    echo "$my_groups" | grep -q "$group" 

    check_r=0
    check_w=0
    if [[ $? -eq 0 ]]; then
        echo "You are on the same group as the port"

        group_read=${permissions:4:1}
        group_write=${permissions:5:1}

        if [[ $DEBUG -eq 1 ]]; then
            echo "Group read: $group_read"
            echo "Group write: $group_write"
        fi

        if [[ "$group_read" == "r" ]]; then
            echo "You can read from local it from your 'group' permission"
            check_r=1
        fi

        if [[ "$group_read" == "w" ]]; then
            echo "You can write in it from your 'group' permission"
            check_w=1
        fi

        if [[ $check_r -eq 1 ]] &&  [[ $check_w -eq 1 ]]; then
            echo "You can write and read from your group permissions"
            continue
        fi

    else
        echo "You are not on the same group of the port"
    fi

    other_read=${permissions:4:1}
    other_write=${permissions:5:1}

    if [[ $DEBUG -eq 1 ]]; then
        echo "Other read: $other_read"
        echo "Other write: $other_write"
    fi

    if [[ "$other_read" == "r" ]]; then
        echo "You can read from it from 'other' permission"
        check_r=1
    fi

    if [[ "$other_write" == "w" ]]; then
        echo "You can write in it from 'other' permission"
        check_w=1
    fi

}


DEBUG=1

# Verifying module 
echo -n "Checking if module is loaded ... "

module="`lsmod | grep virtualbot`"

if [[ $module == "" ]]; then
    echo -e "ERROR!\nKernel module is not loaded!"
    echo "Make sure the module is compiled and installed by\
        running 'make all modules_install install' inside the\
        driver's source code"
    exit 1
else

    echo "ok"

fi

# Verifying Emulated Ports
echo -n "Checking if Emulated Ports are instantiated ..."

emulated_ports_found=`ls -l /dev/ttyEmulatedPort*`

if [[ "$emulated_ports_found" == "" ]]; then
    echo "ERROR! No Emulated Ports found!"
    echo "This might mean there is a problem with the driver itself (not your problem)"
    exit 1
else
    echo "ok"
fi

echo "Checking permissions on the Emulated Ports found..."

for port in $( ls /dev/ttyEmulatedPort* ); do
    echo 
    echo "Port: $port"

    check_permissions $port
    
done


# Verifying Exogenous Ports

