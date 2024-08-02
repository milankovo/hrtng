## Dump strings, comments and names from the .idb
This feature has been made for indexing and searching strings inside `.idb`/`.i64` files.  
The plugin exports IDC functions "`dump_strings`", "`dump_comments`" and "`dump_names`".

You can create `dump_strings.idc` file in `IDADIR/idc` folder with following content:
```
      #include <idc.idc>
      //This script is designed to work in batch mode "idat -A -Sdump_strings.idc $1"
      static main()
      {
        auto flags = get_inf_attr(INF_GENFLAGS);
        flags = flags & ~INFFL_AUTO;        // disable Autoanalysis
        flags = flags | INFFL_READONLY;     //(internal) temporary interdiction to modify the database
        set_inf_attr(INF_GENFLAGS, flags);
        dump_strings();                     //this function is provided by hexrays_tools plugin
        dump_names();                     //this function is provided by hexrays_tools plugin
        //dump_comments();                  //here are a lot of autogenerated comments
        qexit(0);                           // exit to OS, error code 0 - success
      }
```
And shell script `dump_strings_idb.sh` like this:
```
      #/bin/sh
      idapath=~/.local/share/ida
      tmproot=/tmp
      idaexe=idat
      idaexe64=idat64
      tmpdir=$(mktemp -d "$tmproot/idads_XXXXXX")
      fname=$(basename "$1")
      extension="${fname##*.}"
      if test "$extension" = "i64"
      then
        idaexe=$idaexe64
      fi
      cp "$1" "$tmpdir/$fname" || exit 1
      $idapath/$idaexe -A -Sdump_strings.idc "$tmpdir/$fname"
      rm -r "$tmpdir"
```
Install [Recall desktop search tool](www.recoll.org) and [learn it how do dial with idb/i64 files](https://www.lesbonscomptes.com/recoll/usermanual/usermanual.html#RCL.INSTALL.CONFIG.EXAMPLES)  
Then:
 + add to `[index]` section of `~/.recoll/mimeconf` file following line:
```
        application/ida = exec rclidb
```
 + add to `~/.recoll/mimemap` file following lines:
```
        .idb = application/ida
        .i64 = application/ida
```
 + add to `[view]` section of `~/.recoll/mimeview` file following line:
```
        application/ida = (rclidb %f)
```
 + make copy of `/usr/share/recoll/filters/rcltex` file with `rclidb` name
 + modify your `/usr/share/recoll/filters/rclidb` copy after lines:
```
        # !! Leave the following line unmodified !
        #ENDRECFILTCOMMONCODE
```
 + to include following content instead of `TEX` specific
```
        checkcmds dump_strings_idb
        cat <<EOF
        <html><head>
        <meta http-equiv="Content-Type" content="text/html;charset=UTF-8">
        </head><body><pre>
        EOF
        dump_strings_idb "$infile" | sed -e 's/\&/\&amp;/g' -e 's/>/\&gt;/g' -e 's/</\&lt;/g'
        echo '</pre></body></html>'
        exit 0
```
 + wait or force Recall indexing of your idb collection
 + check the search works well
 
![Index and search strings](recoll.gif)

BTW, the same way you can learn Recoll do index and search strings inside binary files, using the following recall filter
```
        # !! Leave the following line unmodified !
        #ENDRECFILTCOMMONCODE
        checkcmds strings sed md5sum
        FLT="sed -e s/\&/\&amp;/g -e s/>/\&gt;/g -e s/</\&lt;/g"
        cat <<EOF
        <html><head>
        <meta http-equiv="Content-Type" content="text/html;charset=UTF-8">
        </head><body><pre>
        EOF
        md5sum -b "$infile"
        strings -n 7 "$infile" | $FLT
        strings -n 5 -el "$infile" | $FLT
        echo '</pre></body></html>'
        exit 0
```