FROM centos:7
WORKDIR /game
#RUN yum install -y g++ make && yum clean all
#RUN yum install -y glibc glibc-devel libgcc && yum clean all
#RUN yum install -y mysql-community-client-libs hiredis && yum clean all
COPY ./mysql-lib/ /usr/lib64/
COPY ./redis-lib/ /usr/lib64/

COPY ./gameserver /game/gameserver
COPY ./config /game/config
COPY ./log /game/log
CMD ["/game/gameserver"]
