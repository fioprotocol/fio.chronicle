#!/usr/bin/env bash

# Set up script environment
SCRIPT_DIR=$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
. ${SCRIPT_DIR}/utils.sh

if [[ "$EUID" -ne 0 ]]; then
  echo && echo "ERROR: Script must be run as root! Use sudo command as follows; sudo ./<script name>"
  echo
  exit 1
fi

if [[ "$(uname)" == "Linux" ]]; then
  if [[ -e /etc/os-release ]]; then
    # obtain NAME and other information
    . /etc/os-release
    if [[ ${NAME} != "Ubuntu" ]]; then
      echo && echo "Currently only supporting Ubuntu based insteall. Proceed at your own risk."
      pause
    fi
  else
    echo && echo "Currently only supporting Ubuntu based install. /etc/os-release not found. Your Linux distribution is not supported. Proceed at your own risk."
    pause
  fi
else
  echo && echo "Currently only supporting Ubuntu based install. Your architecture is not supported. Proceed at your own risk."
  pause
fi

function usage() {
   echo
   printf "Usage: $0 OPTION...
   -d     Install development libraries only (libpq and postgresql-server-dev-16)
   -x     Run in debug mode
   -h     Display usage
   \\n" "$0" 1>&2
   exit 1
}

# Set global vars and get command line options
DEBUG=${DEBUG:-false}
DEV_ONLY=${DEV_ONLY:-false}
UPGRADE_OS=${UPGRADE_OS:-false}
if [ $# -ne 0 ]; then
   while getopts "duxh" opt; do
      # echo "flag -$flag, Argument $OPTARG";
      case "${opt}" in
      d)
         DEV_ONLY=true
         ;;
      u)
         UPGRADE_OS=true
         ;;
      x)
         DEBUG=true
         set -x
         ;;
      h)
         usage
         ;;
      ?)
         echo "Invalid Option!" 1>&2
         usage
         ;;
      :)
         echo "Invalid Option: -${OPTARG} requires an argument." 1>&2
         usage
         ;;
      *)
         usage
         ;;
      esac
   done
fi

# Begin install
POSTGRES_VER=16
echo && echo "PostgreSQL v${POSTGRES_VER} Install"

if ${UPGRADE_OS}; then
  echo && echo "Updating OS..."
  apt update;
  apt upgrade -y;
fi

echo && echo "Installing required packages..."
apt install -y gnupg2 wget vim

echo && echo "Adding the postgres repository..."
sh -c 'echo "deb https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'

echo && echo "Set the signing key for the postgres repository..."
curl -fsSL https://www.postgresql.org/media/keys/ACCC4CF8.asc | gpg --dearmor -o /etc/apt/trusted.gpg.d/postgresql.gpg

echo && echo "Updating package list (again)..."
apt update -y

if ${DEV_ONLY}; then
  echo && echo "Installing PostgreSQL v${POSTGRES_VER}.x development libraries"
  apt install -y postgresql-server-dev-${POSTGRES_VER} libpq-dev
else
  echo && echo "Installing PostgreSQL v${POSTGRES_VER}.x server, client, and development libraries..."
  apt install -y postgresql-${POSTGRES_VER} postgresql-server-dev-${POSTGRES_VER} postgresql-contrib-${POSTGRES_VER} libpq-dev
fi

if [[ $? -ne 0 ]]; then
  echo && echo "ERROR: An error occured installing PostgreSQL!"
  echo "Exiting..."
  exit 1
fi

echo && echo "PostgreSQL v${POSTGRES_VER} has been installed successfully"

if ! ${DEV_ONLY}; then
  echo && echo "In another window, verify the install using the following commands;"
  echo "psql --version OR sudo -u postgres psql -c \"SELECT version();\""
  echo
  echo "Both commands should result in the display of the installed PostgreSQL version. In"
  echo "case of an error, exit this script, determine the failure reason(s) and fix the installation"
  pause

  echo && echo "Enabling and starting the PostgreSQL service..."
  systemctl enable postgresql
  systemctl start postgresql

  echo && echo "Checking service status..."
  sleep 5
  systemctl status postgresql
fi
echo