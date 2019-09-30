FROM debian:testing
WORKDIR /tmp
RUN sed -i s#://deb.debian.org#://cdn-fastly.deb.debian.org# /etc/apt/sources.list \
    && apt-get update \
    && adduser --home /home/travis travis --quiet --disabled-login --gecos "" --uid 1000 \
    && env DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y build-essential apt apt-utils debhelper fakeroot libapt-pkg-dev python-all-dev python-all-dbg python3-all-dev python3-all-dbg python-distutils-extra python-sphinx pep8 pyflakes \
    && apt-get clean
