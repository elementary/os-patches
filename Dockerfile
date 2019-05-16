FROM debian:testing
COPY . /tmp
WORKDIR /tmp
RUN sed -i s#://deb.debian.org#://cdn-fastly.deb.debian.org# /etc/apt/sources.list \
    && apt-get update \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y software-properties-common \
    && add-apt-repository -y ppa:deity/apt  \
    && apt-get update \
    && adduser --home /home/travis travis --quiet --disabled-login --gecos "" --uid 1000 \
    && env DEBIAN_FRONTEND=noninteractive apt-get build-dep -y /tmp \
    && rm -r /tmp/* \
    && apt-get clean
