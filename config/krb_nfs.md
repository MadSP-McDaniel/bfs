# Kerberos & NFS server (192.168.1.1 USER qwerty123)

Kerberos:

REALM: `BFS.COM`
Primary KDC: `kdc01.bfs.com`  
user principal: `principal`
admin principal: `principal/admin`

Add entries to `/etc/hosts` (replace IPs with the correct ones obviously):
```
192.168.1.1   kdc01.bfs.com
192.168.1.1   nfs-server.bfs.com
192.168.1.3   nfs-client.bfs.com
```

`sudo apt install krb5-kdc krb5-admin-server krb5-user libpam-krb5`

Create the new realm
`sudo krb5_newrealm`

Create the first principal and admin principal
```
sudo kadmin.local
> addprinc principal
> addprinc principal/admin
> quit
```
Edit `/etc/krb5kdc/kadm5.acl` with `principal/admin@BFS.COM    *`

Restart service
`sudo systemctl restart krb5-admin-server.service`

Test new principal 
```
kinit principal/admin
klist
```

NFS server:
`sudo apt-get install nfs-kernel-server`

On Kerberos add principals for the nfs server and client:
```
kinit principal/admin
kadmin -q "addprinc -randkey nfs/nfs-server.bfs.com"
kadmin -q "addprinc -randkey nfs/nfs-client.bfs.com"
kadmin -q "addprinc -randkey USER/nfs-client.bfs.com"
```

Now distribute keys on NFS server:
NFS server: `sudo kadmin -p principal/admin -q "ktadd nfs/nfs-server.bfs.com"`

Check that your machine has access to the credentials: `sudo klist -e -k /etc/krb5.keytab`

In `/etc/default/nfs-kernel-server` add: `NEED_SVCGSSD=yes`

Configure `/etc/exports`:
```
/mount_point nfs-client.bfs.com(rw,sec=krb5p,all_squash,anonuid=1000,anongid=1000,subtree_check,sync)
#krb5 = kerberos / krb5i += integrity / krb5p += encryption
```

# NFS Client (192.168.1.3 USER qwerty123)

Add entries to `/etc/hosts` (replace IPs with the correct ones obviously):
```
192.168.1.1   kdc01.bfs.com
192.168.1.1   nfs-server.bfs.com
192.168.1.3   nfs-client.bfs.com
```

`sudo apt-get install nfs-common krb5-user libpam-krb5`
REALM: `BFS.COM`
Primary KDC: `kdc01.bfs.com`  (add entries to `/etc/hosts`)

Now distribute keys on NFS client:
NFS client: `sudo kadmin -p principal/admin -q "ktadd nfs/nfs-client.bfs.com"`
`sudo kadmin -p principal/admin -q "ktadd USER/nfs-client.bfs.com"`

Check that your machine has access to the credentials: `sudo klist -e -k /etc/krb5.keytab`

Mount export: `mount -t nfs4 -o sec=krb5p nfs-server.bfs.com:/mount_point /mount_point`
