<?php

$rootmod = $argv[ 1 ];

function dolib($name,$path,&$mods,$excl,$level = 1)
{
	if (isset($mods[ $name ])) return;
	
	$pad = str_pad("",$level * 4," ");
	
	//
	// Prefer locally builds objects before standard objects
	// when analyzing shared objects.
	//

	$localpath = "/usr/local/lib/" . array_pop(explode("/",$path));

	if (file_exists($localpath))
		exec("ldd $localpath",$lines);
	else
		exec("ldd $path",$lines);
	
	$childs = 0;
	
	foreach ($lines as $line)
	{
		$line = trim($line);
		
		if (substr($line,0,13) == "/lib/ld-linux") continue;
		
		$parts = explode(" ",$line);
		array_pop($parts);
		$line = implode(" ",$parts);
		
		if (isset($excl[ $parts[ 0 ] ])) continue;
				
		echo "$pad$line\n";

		dolib($parts[ 0 ],$parts[ 2 ],$mods,$excl,$level + 1);
		
		$childs++;
	}
	
	$parts = explode(".so",$name);

	$lname = $parts[ 0 ];

	if ($lname == "libts-0.0"         ) $lname = "libts";
	if ($lname == "libSDL-1.2"        ) $lname = "libSDL";
	if ($lname == "libfusion-1.2"     ) $lname = "libfusion";
	if ($lname == "libdirect-1.2"     ) $lname = "libdirect";
	if ($lname == "libdirectfb-1.2"   ) $lname = "libdirectfb";

	$sname = $lname . ".a";

	$mods[ $name ][ "childs" ] = $childs;
	$mods[ $name ][ "shared" ] = $path;
	$mods[ $name ][ "sname"  ] = $sname;

	//
	// Try to identify the matching static archives.
	//

	$test = "/lib/$sname";
	if (file_exists($test)) $mods[ $name ][ "static" ] = $test;

	$test = "/usr/lib/$sname";
	if (file_exists($test)) $mods[ $name ][ "static" ] = $test;

	$test = "/usr/local/lib/$sname";
	if (file_exists($test)) $mods[ $name ][ "static" ] = $test;

	$test = "/lib/arm-linux-gnueabihf/$sname";
	if (file_exists($test)) $mods[ $name ][ "static" ] = $test;

	$test = "/usr/lib/arm-linux-gnueabihf/$sname";
	if (file_exists($test)) $mods[ $name ][ "static" ] = $test;

	$test = "/usr/lib/gcc/arm-linux-gnueabihf/4.8/$sname";
	if (file_exists($test)) $mods[ $name ][ "static" ] = $test;

	//
	// Special hack for pulse
	//

	$test = "/usr/local/lib/pulseaudio/$sname";
	if (file_exists($test)) $mods[ $name ][ "static" ] = $test;
}

$excl = array();

$excl[ "libgcc_s.so.1" ] = true;

$extrashared = "";
$extrastatic = "";
$extraboth   = "";

$parts = explode("/",$rootmod);
array_pop($parts);
array_pop($parts);
$staticpath = implode("/",$parts) . "/ffmpeg-static";

if (file_exists($staticpath))
{
	@exec("rm $staticpath/*");
}

while (true)
{
	$mods = array();

	dolib("ffmpeg",$rootmod,$mods,$excl);

	if (count($mods) < 2) break;
	
	$thisshared = "";
	$thisstatic = "";
	$thisboth   = "";
	
	foreach ($mods as $name => $data)
	{
		if ($data[ "childs" ] > 0) continue;
		
		$sname = $data[ "sname" ];	
		$target = isset($data[ "static" ]) ? $data[ "static" ] : "";

		$isstatic = false;
		
		if ($target == "")
		{
			$parts = explode(".so",$name);
			$static = $parts[ 0 ] . ".a";
			$search = "sudo apt-file search $static";
		
			//echo "$search\n";
			//passthru("sudo apt-file search $static");
		
			//continue;
		}
		else
		{
			if (($sname != "libc.a") &&
			    ($sname != "librt.a") &&
			    ($sname != "libdl.a") &&
			    ($sname != "libgcc_s.a") &&
			    ($sname != "libpthread.a") &&
		 	    ($sname != "libaacplus.a"))
			{
				echo "ln -sf $target .\n";
				if (file_exists($staticpath)) exec("ln -sf $target $staticpath\n");
				
				$isstatic = true;
			}
		}
		
		$excl[ $name ] = true;

		$lname = substr($sname,3,-2);
		
		if ($lname == "glapi") continue;
		
		if ($lname == "ts-0.0"      ) $lname = "ts";
		if ($lname == "SDL-1.2"     ) $lname = "SDL";
		if ($lname == "lber-2.4"    ) $lname = "lber";
		if ($lname == "ldap_r-2.4"  ) $lname = "ldap";

		if ($lname == "ncursesw")
		{
			//
			// Has a hidden dependency to libgpm.a
			//

			$target = "/usr/lib/arm-linux-gnueabihf/libgpm.a";

			echo "ln -sf $target .\n";
			if (file_exists($staticpath)) exec("ln -sf $target $staticpath\n");

			$lname = "ncursesw -lgpm";
		}
		
		if ($isstatic) 
			$thisstatic = $thisstatic . " -l$lname";
		else
			$thisshared = $thisshared . " -l$lname";
		
		$thisboth = $thisboth . " -l$lname";
	}
	
	$extrashared = trim(trim($thisshared) . " " . $extrashared);
	$extrastatic = trim(trim($thisstatic) . " " . $extrastatic);
	$extraboth   = trim(trim($thisboth)   . " " . $extraboth  );
}

echo "EXTRALIBS=-L../ffmpeg-static $extrastatic";
echo " ";
echo "$extrashared";
echo "\n";
echo "\n";

echo "EXTRALIBS=-L../ffmpeg-static $extraboth";
echo "\n";

?>
