IPTABLES=iptables
NAT=nat
LO=lo
ETH=eth0
ETH1=eth1
modprobe ip_conntrack_ftp
modprobe ip_nat_ftp
$IPTABLES -F -t $NAT
$IPTABLES -F
$IPTABLES -P FORWARD ACCEPT
$IPTABLES -P INPUT ACCEPT
$IPTABLES -P OUTPUT ACCEPT

# only accept the following subnet
$IPTABLES -A FORWARD -s 10.49.188.0/24 -j ACCEPT
$IPTABLES -A FORWARD -s 10.49.1.23 -j ACCEPT
$IPTABLES -A FORWARD -s 172.18.0.0/16 -j ACCEPT


# for ykt
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 172.18.0.0/16 -d 10.49.188.244 --dport 8383 -o $ETH -j MASQUERADE

$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.13 -d 172.18.0.0/16 --dport 22 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.13 -d 172.18.0.0/16 --dport 21 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.13 -d 172.18.0.0/16 --dport 50002 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.13 -d 172.0.0.0/8 --dport 4899 -o $ETH1 -j MASQUERADE
#$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.13 -d 172.0.0.0/8 -o $ETH1 -j MASQUERADE

$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.99 -d 172.18.109.101 --dport 50002 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.50 -d 172.18.109.101 --dport 50002 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.20 -d 172.18.109.101 --dport 50002 -o $ETH1 -j MASQUERADE
#$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.240.0.123 -d 172.18.109.101 --dport 50002 -o $ETH1 -j MASQUERADE

$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.111 -d 172.18.0.0/16 --dport 22 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.111 -d 172.18.0.0/16 --dport 21 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.111 -d 172.18.0.0/16 --dport 50002 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.111 -d 172.18.0.0/16 --dport 4899 -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -p tcp -s 10.49.188.111 -d 172.0.0.0/8 --dport 10001 -o $ETH1 -j MASQUERADE

# for subversion forward
$IPTABLES -t $NAT -A PREROUTING -p tcp -s 172.18.0.0/16 --dport 8383 -j DNAT --to 10.49.188.244


# common config
$IPTABLES -t $NAT -A POSTROUTING -m state --state RELATED,ESTABLISHED -o $ETH1 -j MASQUERADE
$IPTABLES -t $NAT -A POSTROUTING -s 10.49.188.0/24 -d 172.18.0.0/16 -o $ETH1 -j SNAT --to 172.18.109.254
#$IPTABLES -t $NAT -A POSTROUTING -s 10.49.1.23 -d 172.18.0.0/16 -o $ETH1 -j SNAT --to 172.18.109.254

# for office network
$IPTABLES -A INPUT -s 10.49.188.0/24 -j ACCEPT
$IPTABLES -A INPUT -s 172.18.0.0/16 -j ACCEPT
$IPTABLES -A INPUT -s 10.108.0.222 -j ACCEPT
$IPTABLES -A INPUT -s 10.108.0.70 -j ACCEPT
$IPTABLES -A INPUT -s 10.108.0.1 -j ACCEPT
#$IPTABLES -A INPUT -s 10.108.0.1 -j ACCEPT
$IPTABLES -A INPUT -j ACCEPT -i $LO
# for internet 
# !!!!! must be the lastest
$IPTABLES -t $NAT -A POSTROUTING -s 10.49.188.0/24 -o $ETH -j MASQUERADE
#$IPTABLES -t $NAT -A POSTROUTING -s 10.49.188.0/24 -j SNAT --to 10.49.188.211
