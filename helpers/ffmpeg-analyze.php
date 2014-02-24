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
				
		//echo "$pad$line\n";

		dolib($parts[ 0 ],$parts[ 2 ],$mods,$excl,$level + 1);
		
		$childs++;
	}
	
	$parts = explode(".so",$name);
	$sname = $parts[ 0 ] . ".a";

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
}

$excl = array();

$excl[ "libgcc_s.so.1" ] = true;

$extrashared = "";
$extrastatic = "";
$extraboth   = "";

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
			if (($sname != "libcelt0.a") &&
		 	    ($sname != "libaacplus.a"))
			{
				echo "ln -sf $target .\n";
				
				$isstatic = true;
			}
		}
		
		$excl[ $name ] = true;

		$lname = substr($sname,3,-2);
		
		if ($lname == "pulsecommon-4.0") continue;
		if ($lname == "glapi") continue;
		
		if ($lname == "ts-0.0"      ) $lname = "ts";
		if ($lname == "SDL-1.2"     ) $lname = "SDL";
		if ($lname == "lber-2.4"    ) $lname = "lber";
		if ($lname == "ldap_r-2.4"  ) $lname = "ldap";
		if ($lname == "fusion-1.2"  ) $lname = "fusion";
		if ($lname == "direct-1.2"  ) $lname = "direct";
		if ($lname == "directfb-1.2") $lname = "directfb";

		if ($lname == "ncursesw"    ) $lname = "ncursesw -lgpm";
		
		if ($isstatic) 
			$thisstatic = $thisstatic . " -l$lname";
		else
			$thisshared = $thisshared . " -l$lname";
		
		$thisboth = $thisstatic . " -l$lname";
	}
	
	$extrashared = trim($thisshared) . " " . $extrashared;
	$extrastatic = trim($thisstatic) . " " . $extrastatic;
	$extraboth   = trim($thisboth)   . " " . $extraboth;
}

echo "$extrastatic";
echo " ";
echo "$extrashared";

//echo "$extraboth";

echo "\n";

?>
