FROM centos:7
LABEL Name="jec-test"
MAINTAINER James Clough <jclough@cray.com>
RUN yum -y install openssl-devel python-devel java-1.8.0-openjdk which unzip wget epel-release kernel-headers kernel-devel kernel-syms rsync perl rpm make vim sudo openssh-clients yum-utils device-mapper-persistent-data lvm2 python-setuptools python-yaml
RUN yum -y groupinstall 'Development Tools'
RUN yum -y install postfix
RUN sed -i 's/.*mydestination = $myhostname, localhost.$mydomain.*/mydestination = $myhostname, localhost.$mydomain, $mydomain/' /etc/postfix/main.cf
RUN sed -i 's/.*inet_interfaces = .*/inet_interfaces = loopback-only/' /etc/postfix/main.cf
RUN cp /etc/profile /etc/profile_backup
RUN echo 'export JAVA_HOME=/usr/lib/jvm/jre-1.8.0-openjdk' | tee -a /etc/profile
RUN echo 'export JRE_HOME=/usr/lib/jvm/jre' | tee -a /etc/profile
RUN source /etc/profile
RUN mkdir -p /tmp/src
COPY boottime  cmsr.c  cray-cmsr.spec  Makefile /tmp/src/.
RUN cd ~
