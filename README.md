# bsdiff-php
PHP extensions for bsdiff

## Build
```
phpize
./configure --with-php-config=/usr/bin/php-config
make
make install
```

## Setup
* Modify PHP Config
    * vi /etc/php/7.3/apache2/
        * Enable the Extension
        ```
            extension=bsdiff
        ```
    * Save
* Reboot the Server
* Configure bsdiff Directories
```
mkdir -p ~/Desktop/php
chown -R www-data:www-data ~/Desktop/php
chmod -R 755 ~/Desktop/php
```

## Usage
```
<?php
    ini_set('memory_limit', '700M');
    set_time_limit(0);
    error_reporting(E_ALL);

    if (bsdiff_diff("old.apk", "new.apk", "apk.patch"))
    {
        echo "bdiff success.\n";
    }
    else
    {
        echo "bdiff failed.\n";
        exit(1);
    }

    if (bsdiff_patch("old.apk", "apk.patch", "a.new.apk"))
    {
            echo "bpatch success.\n";
    }
    else
    {
            echo "bpatch failed.\n";
    }
?>
```
## Credit
http://www.daemonology.net/bsdiff/
