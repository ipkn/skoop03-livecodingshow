import urllib
key = open("key").read()
urllib.urlopen("http://localhost:40080/reload", data=key).read()
