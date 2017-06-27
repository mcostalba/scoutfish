FROM debian:wheezy
RUN apt-get update && apt-get install -y make g++
VOLUME /home/builder/scoutfish
WORKDIR /home/builder/scoutfish
RUN groupadd -r builder && useradd -r -g builder builder
USER builder
CMD ./linux_build.sh
