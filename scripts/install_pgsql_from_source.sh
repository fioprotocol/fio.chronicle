#!/usr/bin/env bash

# PostgreSQL installation from source
# This script compiles and installs PostgreSQL from source code

set -e  # Exit on error

POSTGRES_VER=${1:-16.7}  # Accept version as argument, default to 16.7
POSTGRES_MAJOR_VER=$(echo ${POSTGRES_VER} | cut -d. -f1)

echo && echo "PostgreSQL v${POSTGRES_VER} Source Installation"

if [[ "$EUID" -ne 0 ]]; then
  echo && echo "ERROR: Script must be run as root! Use sudo command"
  exit 1
fi

echo && echo "Installing build dependencies..."
apt update
apt install -y \
    build-essential \
    libreadline-dev \
    zlib1g-dev \
    flex \
    bison \
    libxml2-dev \
    libxslt1-dev \
    libssl-dev \
    libxml2-utils \
    xsltproc \
    libkrb5-dev \
    libldap2-dev \
    libpam0g-dev \
    python3-dev \
    tcl-dev \
    libperl-dev \
    gettext \
    uuid-dev \
    libicu-dev \
    pkg-config \
    wget

echo && echo "Downloading PostgreSQL ${POSTGRES_VER}..."
cd /tmp
wget -q https://ftp.postgresql.org/pub/source/v${POSTGRES_VER}/postgresql-${POSTGRES_VER}.tar.gz

if [[ ! -f postgresql-${POSTGRES_VER}.tar.gz ]]; then
    echo "ERROR: Failed to download PostgreSQL ${POSTGRES_VER}"
    exit 1
fi

echo && echo "Extracting..."
tar xzf postgresql-${POSTGRES_VER}.tar.gz
cd postgresql-${POSTGRES_VER}

echo && echo "Configuring PostgreSQL..."
./configure \
    --prefix=/usr/local/pgsql \
    --with-openssl \
    --with-libxml \
    --with-libxslt \
    --with-icu \
    --with-ldap \
    --with-pam \
    --with-perl \
    --with-python \
    --with-tcl \
    --with-uuid=e2fs

if [[ $? -ne 0 ]]; then
    echo "ERROR: Configuration failed!"
    exit 1
fi

echo && echo "Compiling PostgreSQL (this may take 10-15 minutes)..."
make -j$(nproc)

if [[ $? -ne 0 ]]; then
    echo "ERROR: Compilation failed!"
    exit 1
fi

echo && echo "Installing PostgreSQL..."
make install

# Install contrib modules
echo && echo "Installing contrib modules..."
cd contrib
make -j$(nproc)
make install

# Install development headers and libraries
echo && echo "Installing development files..."
cd /tmp/postgresql-${POSTGRES_VER}
make install-world-bin

echo && echo "Creating postgres user..."
if ! id -u postgres &>/dev/null; then
    useradd -m -d /var/lib/postgresql -s /bin/bash postgres
fi

echo && echo "Setting up environment..."
cat > /etc/profile.d/postgresql.sh <<'EOF'
export PATH=/usr/local/pgsql/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/pgsql/lib:$LD_LIBRARY_PATH
export MANPATH=/usr/local/pgsql/share/man:$MANPATH
EOF

# Source the environment for current session
source /etc/profile.d/postgresql.sh

# Update ldconfig
echo "/usr/local/pgsql/lib" > /etc/ld.so.conf.d/postgresql.conf
ldconfig

# Check if PostgreSQL is already initialized
PGDATA="/var/lib/postgresql/data"
NEEDS_INIT=false

if [[ -d "$PGDATA" ]]; then
    if [[ ! -f "$PGDATA/PG_VERSION" ]]; then
        echo && echo "Data directory exists but is not initialized (or incomplete)."
        echo "Removing and recreating..."
        rm -rf "$PGDATA"
        NEEDS_INIT=true
    else
        echo && echo "PostgreSQL data directory already initialized."
        echo "Skipping initdb..."
        NEEDS_INIT=false
    fi
else
    NEEDS_INIT=true
fi

if [[ "$NEEDS_INIT" == "true" ]]; then
    echo && echo "Creating data directory..."
    mkdir -p "$PGDATA"
    chown -R postgres:postgres /var/lib/postgresql
    
    echo && echo "Initializing database cluster..."
    su - postgres -c "/usr/local/pgsql/bin/initdb -D $PGDATA"
    
    echo && echo "Configuring PostgreSQL to listen on all interfaces..."
    su - postgres -c "echo \"listen_addresses = '*'\" >> $PGDATA/postgresql.conf"
    su - postgres -c "echo \"host all all 0.0.0.0/0 md5\" >> $PGDATA/pg_hba.conf"
else
    # Just ensure ownership is correct
    chown -R postgres:postgres /var/lib/postgresql
fi

# Check if systemd is available
if command -v systemctl &> /dev/null; then
    echo && echo "Creating systemd service..."
    cat > /etc/systemd/system/postgresql.service <<EOF
[Unit]
Description=PostgreSQL ${POSTGRES_MAJOR_VER} database server
Documentation=man:postgres(1)
After=network.target

[Service]
Type=notify
User=postgres
Group=postgres
Environment=PGDATA=/var/lib/postgresql/data
ExecStart=/usr/local/pgsql/bin/postgres -D \${PGDATA}
ExecReload=/bin/kill -HUP \$MAINPID
KillMode=mixed
KillSignal=SIGINT
TimeoutSec=infinity

# Restart policy
Restart=on-failure
RestartSec=5s

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/postgresql

[Install]
WantedBy=multi-user.target
EOF

    echo && echo "Reloading systemd and enabling PostgreSQL service..."
    systemctl daemon-reload
    systemctl enable postgresql
    
    # Check if PostgreSQL is already running
    if systemctl is-active --quiet postgresql; then
        echo "PostgreSQL is already running. Restarting..."
        systemctl restart postgresql
    else
        echo "Starting PostgreSQL..."
        systemctl start postgresql
    fi
    
    echo && echo "Waiting for PostgreSQL to start..."
    sleep 3
else
    echo && echo "Systemd not available. Creating SysV init script..."
    
    cat > /etc/init.d/postgresql <<'INITEOF'
#!/bin/bash
### BEGIN INIT INFO
# Provides:          postgresql
# Required-Start:    $local_fs $remote_fs $network $time
# Required-Stop:     $local_fs $remote_fs $network $time
# Should-Start:      $syslog
# Should-Stop:       $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: PostgreSQL database server
### END INIT INFO

PGDATA="/var/lib/postgresql/data"
PGUSER="postgres"
PGCTL="/usr/local/pgsql/bin/pg_ctl"
PGLOG="/var/lib/postgresql/logfile"

case "$1" in
    start)
        echo "Starting PostgreSQL..."
        su - $PGUSER -c "$PGCTL start -D $PGDATA -l $PGLOG"
        ;;
    stop)
        echo "Stopping PostgreSQL..."
        su - $PGUSER -c "$PGCTL stop -D $PGDATA"
        ;;
    restart)
        echo "Restarting PostgreSQL..."
        su - $PGUSER -c "$PGCTL restart -D $PGDATA -l $PGLOG"
        ;;
    reload)
        echo "Reloading PostgreSQL..."
        su - $PGUSER -c "$PGCTL reload -D $PGDATA"
        ;;
    status)
        su - $PGUSER -c "$PGCTL status -D $PGDATA"
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|reload|status}"
        exit 1
        ;;
esac

exit 0
INITEOF

    chmod +x /etc/init.d/postgresql
    
    # Try to enable service using update-rc.d if available
    if command -v update-rc.d &> /dev/null; then
        update-rc.d postgresql defaults
    fi
    
    # Check if PostgreSQL is already running
    if service postgresql status &> /dev/null; then
        echo && echo "PostgreSQL is already running. Restarting..."
        service postgresql restart
    else
        echo && echo "Starting PostgreSQL..."
        service postgresql start
    fi
    
    echo && echo "Waiting for PostgreSQL to start..."
    sleep 3
fi

echo && echo "Verifying installation..."
if su - postgres -c '/usr/local/pgsql/bin/psql -c "SELECT version();"' 2>/dev/null; then
    echo "PostgreSQL is running successfully!"
else
    echo "WARNING: Could not connect to PostgreSQL. It may still be starting..."
fi

echo && echo "Cleaning up..."
rm -rf /tmp/postgresql-${POSTGRES_VER}*

echo && echo "PostgreSQL ${POSTGRES_VER} has been successfully installed from source!"
echo "PostgreSQL binaries location: /usr/local/pgsql/bin"
echo "Data directory: /var/lib/postgresql/data"

if command -v systemctl &> /dev/null; then
    echo "Service control (systemd):"
    echo "  systemctl start postgresql"
    echo "  systemctl stop postgresql"
    echo "  systemctl status postgresql"
else
    echo "Service control (SysV init):"
    echo "  service postgresql start"
    echo "  service postgresql stop"
    echo "  service postgresql restart"
    echo "  service postgresql status"
fi

echo
echo "To connect: psql -U postgres"

exit 0