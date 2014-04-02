<?php

$if = fopen("/dev/mmcblk0","r");
$of = fopen("/dev/sda","w");
$bs = 10 * 1024 * 1024;
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
