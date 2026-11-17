#!/usr/bin/env bash
set -euo pipefail
mkdir -p evidence
# The regional EC2 mirror stalled for the complete 15-minute job in run
# 44201098856. Keep the same Ubuntu packages, use the canonical mirror.
for sources in /etc/apt/sources.list /etc/apt/sources.list.d/ubuntu.sources; do
 if [ -f "$sources" ]; then
  sudo sed -i -E 's#https?://[^ /]*ec2\.archive\.ubuntu\.com/ubuntu/?#https://archive.ubuntu.com/ubuntu#g' "$sources"
 fi
done
apt_options=(-o Acquire::Retries=2 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30)
sudo apt-get "${apt_options[@]}" update
sudo apt-get "${apt_options[@]}" install -y --no-install-recommends libopencv-dev python3-opencv xvfb xdotool imagemagick
dpkg-query -W libopencv-dev python3-opencv xvfb xdotool imagemagick > evidence/dependencies.txt
