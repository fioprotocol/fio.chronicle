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
POSTGRES_FULL_VER=16.7  # For source installation
echo && echo "PostgreSQL v${POSTGRES_VER} Install"

# Check if PostgreSQL is already installed
# Check both standard PATH and source installation path
PSQL_CMD=""
if command -v psql &> /dev/null; then
  PSQL_CMD="psql"
elif [[ -x /usr/local/pgsql/bin/psql ]]; then
  PSQL_CMD="/usr/local/pgsql/bin/psql"
fi

if [[ -n "${PSQL_CMD}" ]]; then
  # Extract major version number (e.g., "psql (PostgreSQL) 16.7" -> "16")
  INSTALLED_VER=$(${PSQL_CMD} --version 2>/dev/null | sed -n 's/.*PostgreSQL[^0-9]*\([0-9]*\).*/\1/p')
  echo && echo "Detected existing PostgreSQL installation: $(${PSQL_CMD} --version 2>/dev/null)"
  echo "Installed major version: ${INSTALLED_VER}"
  
  if [[ "${INSTALLED_VER}" == "${POSTGRES_VER}" ]]; then
    echo && echo "PostgreSQL ${POSTGRES_VER} is already installed."
    echo "Skipping installation. If you want to reinstall, please uninstall first."
    exit 0
  elif [[ -n "${INSTALLED_VER}" ]]; then
    echo && echo "WARNING: PostgreSQL ${INSTALLED_VER} is already installed (requested: ${POSTGRES_VER})."
    echo "Proceeding may cause conflicts. Consider uninstalling the existing version first."
    pause
  fi
fi

if ${UPGRADE_OS}; then
  echo && echo "Updating OS..."
  apt update;
  apt upgrade -y;
fi

echo && echo "Installing required packages..."
apt install -y gnupg2 wget vim lsb-release

# Check Ubuntu version
OS_CODENAME=$(lsb_release -cs)
echo && echo "Detected OS: ${OS_CODENAME}"

# Auto-detect installation method based on OS
if [[ "${OS_CODENAME}" == "focal" ]]; then
  echo && echo "Ubuntu 20.04 detected."
  echo "PostgreSQL ${POSTGRES_VER} packages are not available for focal."
  echo "Installing from source..."
  echo
  ${SCRIPT_DIR}/install_pgsql_from_source.sh ${POSTGRES_FULL_VER}
  exit $?
fi

# Standard package installation for Ubuntu 22.04+
echo && echo "Adding the postgres repository..."
sh -c "echo \"deb https://apt.postgresql.org/pub/repos/apt ${OS_CODENAME}-pgdg main\" > /etc/apt/sources.list.d/pgdg.list"

echo && echo "Set the signing key for the postgres repository..."
curl -fsSL https://www.postgresql.org/media/keys/ACCC4CF8.asc | gpg --dearmor -o /etc/apt/trusted.gpg.d/postgresql.gpg

echo && echo "Updating package list (again)..."
apt update -y

if ${DEV_ONLY}; then
  echo && echo "Installing PostgreSQL v${POSTGRES_VER}.x development library"
  apt install -y libpq-dev
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

  # Check if systemd is available (not available in Docker or some EC2 instances)
  if command -v systemctl &> /dev/null && systemctl --version &> /dev/null; then
    echo && echo "Enabling and starting the PostgreSQL service (systemd)..."
    systemctl enable postgresql
    systemctl start postgresql

    echo && echo "Checking service status..."
    sleep 5
    systemctl status postgresql
  elif command -v service &> /dev/null; then
    echo && echo "Starting the PostgreSQL service (SysV/service)..."
    service postgresql start

    echo && echo "Checking service status..."
    sleep 5
    service postgresql status || true
  else
    echo && echo "WARNING: Neither systemctl nor service command found."
    echo "You may need to start PostgreSQL manually."
    echo "Try: pg_ctlcluster ${POSTGRES_VER} main start"
  fi
fi
echo