FROM debian:testing
COPY . /tmp
WORKDIR /tmp
RUN sed -i s#://deb.debian.org#://cdn-fastly.deb.debian.org# /etc/apt/sources.list \
    && apt-get update \
    && adduser --home /home/travis travis --quiet --disabled-login --gecos "" --uid 1000 \
    && env DEBIAN_FRONTEND=noninteractive apt-get build-dep -y /tmp \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y ccache python3-pip \
    && python3 -m pip install -U mypy \
    && rm -r /tmp/* \
    && apt-get clean
