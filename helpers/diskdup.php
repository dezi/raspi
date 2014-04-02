<?php

$dv = readlink("/dev/disk/by-label/rootfs");
if (substr($dv,0,6) != "../../") exit(1);

$id = "/dev/mmcblk0";
$od = "/dev/" . substr($dv,6,-1);
$bs = 10 * 1024 * 1024;

echo "dd if=$id of=$od bs=$bs\n";

$if = fopen($id,"r");
$of = fopen($od,"w");
$bc = 0;

while (! feof($if))
{
	$buffer = fread($if,$bs);
	if ($buffer === false) break;

	fwrite($of,$buffer,strlen($buffer));

	$bc += 10;

	echo str_pad($bc,6," ",STR_PAD_LEFT) .  " MB\r";
}

echo "Done...\n";

fclose($if);
fclose($of);

?>
