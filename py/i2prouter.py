#!/usr/bin/env python3
#
# this script generates initial keys and populates netdb as needed
#
import i2py
import sqlite3, os
from urllib.request import urlopen as urlget

def get_external_ip():
    print ('get external ip address')
    return urlget('http://ipecho.net/plain').read().strip().decode('ascii')

def has_config(cur,k):
    return cur.execute('SELECT value FROM config WHERE name = ?',(k,)).fetchone() is not None

def put_config(cur,k,v):
    cur.execute('INSERT INTO config ( name, value ) VALUES ( ? , ? )',(k,v))

def cp(src,dst):
    with open(src,'rb') as r:
        with open(dst,'wb+') as w:
            w.write(r.read())

def migrate_netdb(new_dir='netDb'):
    netdb_dir = os.path.join(os.environ['HOME'],'.i2p','netDb')
    if os.path.exists(netdb_dir):
        print ('Migrate netdb...')
        if not os.path.exists(new_dir):
            os.mkdir(new_dir)
        print ('imported %d entries'%import_netdb_dir(netdb_dir,new_dir))

def import_netdb_dir(netdb_dir,new_dir):
    count = 0
    for root , dirs, files in os.walk(netdb_dir):
        if len(dirs) > 0:
            for dir in dirs:
                count += import_netdb_dir(os.path.join(root,dir),new_dir)
        for file in files:
            cp(os.path.join(root,file),os.path.join(new_dir,file))
            count += 1
    return count 
def init(db_schema='db_schema.sql',db_fname='i2p.db',ssu_ip='0.0.0.0',ssu_port=6699,max_peers=10):
    con = sqlite3.connect(db_fname)
    cur = con.cursor()

    if not os.path.exists(db_schema):
        print('cannot find '+db_schema)
        return

    with open(db_schema) as r:
        for line in r.read().split(';'):
            line = line.replace('\n',' ').replace('\r',' ').replace('\t',' ').strip()
            cur.execute(line)
        con.commit()

    if not ( has_config(cur,'private_encryption_key') and has_config(cur,'private_signing_key')):
        print ('generate keys')
        signing_pub , signing_prv , encryption_pub, encryption_prv = i2py.gen_keys()
        put_config(cur,'private_encryption_key',encryption_prv)
        put_config(cur,'private_signing_key',signing_prv)

    if not has_config(cur,'ssu_bind_port'):
        print ('set ssu port to %d'%ssu_port)
        put_config(cur,'ssu_bind_port',ssu_port)

    if not has_config(cur,'ssu_bind_ip'):
        print ('set ssu ip to '+ssu_ip)
        put_config(cur,'ssu_bind_ip',ssu_ip)

    if not has_config(cur,'ssu_external_port'):
        print ('set ssu external port to %d'%ssu_port)
        put_config(cur,'ssu_external_port',ssu_port)    

    if not has_config(cur,'ssu_external_ip'):
        extern_ip = get_external_ip()
        print ('set ssu external ip to '+extern_ip)
        put_config(cur,'ssu_external_ip',extern_ip)
        
    if not has_config(cur,'max_peers'):
        print ('set max peer count to %d'%max_peers)
        put_config(cur,'max_peers',max_peers)
    
    con.commit()
    con.close()
    migrate_netdb()


if __name__ == '__main__':
    init(max_peers=50)
    os.system('./i2p')