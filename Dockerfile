FROM centos:7
LABEL Name="jec-test"
MAINTAINER James Clough <jclough@cray.com>
RUN yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
RUN yum -y install openssl-devel python-devel java-1.8.0-openjdk which unzip wget epel-release kernel-headers kernel-devel rsync perl rpm make vim sudo openssh-clients docker-ce mailx yum-utils device-mapper-persistent-data lvm2 supervisor python-setuptools mailutils python-yaml
RUN yum -y groupinstall 'Development Tools'
RUN yum -y install postfix
#RUN easy_install supervisor
#RUN mkdir -p /var/log/supervisor
#COPY supervisord.conf /etc/supervisord.conf
#COPY jenkins /etc/sysconfig/jenkins
RUN sed -i 's/.*mydestination = $myhostname, localhost.$mydomain.*/mydestination = $myhostname, localhost.$mydomain, $mydomain/' /etc/postfix/main.cf
RUN sed -i 's/.*inet_interfaces = .*/inet_interfaces = loopback-only/' /etc/postfix/main.cf
RUN cp /etc/profile /etc/profile_backup
RUN echo 'export JAVA_HOME=/usr/lib/jvm/jre-1.8.0-openjdk' | tee -a /etc/profile
RUN echo 'export JRE_HOME=/usr/lib/jvm/jre' | tee -a /etc/profile
RUN source /etc/profile
RUN cd ~
RUN sudo wget -O /etc/yum.repos.d/jenkins.repo https://pkg.jenkins.io/redhat-stable/jenkins.repo
RUN sudo rpm --import https://pkg.jenkins.io/redhat-stable/jenkins.io.key
RUN sudo yum -y install jenkins
#COPY jenkins /etc/sysconfig/jenkins
RUN chown -c jenkins:jenkins /var/lib/jenkins
RUN chsh -s /bin/bash jenkins
RUN yum -y install docker-ce
RUN echo 'jenkins ALL=(ALL) NOPASSWD: ALL' | sudo EDITOR='tee -a' visudo
RUN usermod -aG docker jenkins
USER jenkins
#CMD ["sudo", "/usr/bin/supervisord", "-c" ,"/etc/supervisord.conf"]


