# 1. 写入新的配置文件，包含多个可用的镜像源
sudo cat > /etc/docker/daemon.json <<EOF
{
  "registry-mirrors": [
    "https://docker.1ms.run",
    "https://docker-0.unsee.tech",
    "https://docker.m.daocloud.io"
  ]
}
EOF

# 2. 重新加载并重启 Docker
sudo systemctl daemon-reload
sudo systemctl restart docker

# 3. 验证配置是否生效 (应该看到新配置的三个地址)
sudo docker info | grep -A 5 "Registry Mirrors"

# 4. 再次尝试拉取 CentOS 7 镜像
sudo docker pull centos:7
