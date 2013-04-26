<?php

include "json.php";

function icy_init()
{	
	$icyjson = file_get_contents("./icyscan.json");
	$icyscan = json_decdat($icyjson);
		
	if (isset($icyscan[ "reverse" ]))
	{
		$reverse = Array();
		
		foreach ($icyscan[ "reverse" ] as $channel)
		{
			if (! strlen($channel)) continue;
			$reverse[ $channel ] = true;
		}
		
		$GLOBALS[ "reverse" ] = &$reverse;
	}
	
	if (isset($icyscan[ "ignore" ]))
	{
		$ignore = Array();
		
		foreach ($icyscan[ "ignore" ] as $channel)
		{
			if (! strlen($channel)) continue;
			$ignore[ $channel ] = true;
		}
		
		$GLOBALS[ "ignore" ] = &$ignore;
	}

	$fixups = Array();
	
	if (isset($icyscan[ "fixups" ]))
	{
		if (isset($icyscan[ "fixups" ][ "str_replace" ]))
		{
			$str_replace = Array();
			
			foreach ($icyscan[ "fixups" ][ "str_replace" ] as $line)
			{
				if (! strlen($line)) continue;
				
				$first = substr($line,0,1);
				$parts = explode($first,$line);
				$parts[ 0 ] = $first;
				$channel = $parts[ 1 ];
				
				if (! isset($str_replace[ $channel ])) $str_replace[ $channel ] = Array();
				
				array_push($str_replace[ $channel ],$parts);
			}
			
			$fixups[ "str_replace" ] = &$str_replace;
		}
		
		if (isset($icyscan[ "fixups" ][ "preg_replace" ]))
		{
			$preg_replace = Array();
			
			foreach ($icyscan[ "fixups" ][ "preg_replace" ] as $line)
			{
				if (! strlen($line)) continue;
				
				$first = substr($line,0,1);
				$parts = explode($first,$line);
				$parts[ 0 ] = $first;
				$channel = $parts[ 1 ];
				
				if (! isset($preg_replace[ $channel ])) $preg_replace[ $channel ] = Array();
				
				array_push($preg_replace[ $channel ],$parts);
			}
			
			$fixups[ "preg_replace" ] = &$preg_replace;
		}
	}
	
	$GLOBALS[ "fixups" ] = &$fixups;
	
	$nuke = Array();
	
	if (isset($icyscan[ "nuke" ]))
	{
		if (isset($icyscan[ "nuke" ][ "strstr" ]))
		{
			$strstr = Array();
			
			foreach ($icyscan[ "nuke" ][ "strstr" ] as $line)
			{
				if (! strlen($line)) continue;
				
				$first = substr($line,0,1);
				$parts = explode($first,$line);
				$parts[ 0 ] = $first;
				$channel = $parts[ 1 ];
				
				if (! isset($strstr[ $channel ])) $strstr[ $channel ] = Array();
				
				array_push($strstr[ $channel ],$parts);
			}
			
			$nuke[ "strstr" ] = &$strstr;
		}
		
		if (isset($icyscan[ "nuke" ][ "preg_match" ]))
		{
			$preg_match = Array();
			
			foreach ($icyscan[ "nuke" ][ "preg_match" ] as $line)
			{
				if (! strlen($line)) continue;
				
				$first = substr($line,0,1);
				$parts = explode($first,$line);
				$parts[ 0 ] = $first;
				$channel = $parts[ 1 ];
				
				if (! isset($preg_match[ $channel ])) $preg_match[ $channel ] = Array();
				
				array_push($preg_match[ $channel ],$parts);
			}
			
			$nuke[ "preg_match" ] = &$preg_match;
		}
	}
	
	$GLOBALS[ "nuke" ] = &$nuke;
}

function fixups_icy($channel,$icy)
{
	if (isset($GLOBALS[ "fixups" ]) &&
	    isset($GLOBALS[ "fixups" ][ "str_replace" ]) &&
		isset($GLOBALS[ "fixups" ][ "str_replace" ][ $channel ]))
	{
		foreach ($GLOBALS[ "fixups" ][ "str_replace" ][ $channel ] as $parts)
		{
			$from = $parts[ 2 ];
			$toto = $parts[ 3 ];
			
			echo "====> fixups str_replace($from,$toto) => $icy\n";
			$icy = trim(str_replace($from,$toto,$icy));
		}
	}
	
	if (isset($GLOBALS[ "fixups" ]) &&
	    isset($GLOBALS[ "fixups" ][ "preg_replace" ]) &&
		isset($GLOBALS[ "fixups" ][ "preg_replace" ][ $channel ]))
	{
		foreach ($GLOBALS[ "fixups" ][ "preg_replace" ][ $channel ] as $parts)
		{
			$sepa = $parts[ 0 ];
			$from = $parts[ 2 ];
			$toto = $parts[ 3 ];
			$modi = $parts[ 4 ];
			
			$pattern = $sepa . $from . $sepa . $modi;
			
			echo "====> fixups preg_replace($pattern) => $icy\n";
			$icy = trim(preg_replace($pattern,$toto,$icy));
		}
	}
	
	return $icy;
}

function nuke_icy($channel,$icy)
{
	if (isset($GLOBALS[ "nuke" ]) &&
	    isset($GLOBALS[ "nuke" ][ "strstr" ]) &&
		isset($GLOBALS[ "nuke" ][ "strstr" ][ $channel ]))
	{
		foreach ($GLOBALS[ "nuke" ][ "strstr" ][ $channel ] as $parts)
		{
			$pattern = $parts[ 2 ];
			
			if (strstr($icy,$pattern) !== false) 
			{
				echo "====> nuke strstr($pattern) => $icy\n";
				
				return true;
			}
		}
	}
	
	if (isset($GLOBALS[ "nuke" ]) &&
	    isset($GLOBALS[ "nuke" ][ "preg_match" ]) &&
		isset($GLOBALS[ "nuke" ][ "preg_match" ][ $channel ]))
	{
		foreach ($GLOBALS[ "nuke" ][ "preg_match" ][ $channel ] as $parts)
		{
			$sepa = $parts[ 0 ];
			$from = $parts[ 2 ];
			$modi = $parts[ 3 ];
			
			$pattern = $sepa . $from . $sepa . $modi;
			
			if (! preg_match($pattern,$icy)) 
			{			
				echo "====> nuke preg_match($pattern) => $icy\n";

				return true;
			}
		}
	}

	return false;
}

function nice_fup($icy)
{
	$tst = " " . utf8_decode($icy);
	$len = strlen($tst);
	$icy = "";
	
	for ($inx = 1; $inx < $len; $inx++)
	{
		if (ctype_upper($tst[ $inx ]) && 
			(ctype_alnum($tst[ $inx - 1 ]) || 
			($tst[ $inx - 1 ] == "'") ||
			($tst[ $inx - 1 ] == "`")))
		{
			$icy .= strtolower($tst[ $inx ]);
		}
		else
		{
			$icy .= $tst[ $inx ];
		}
	}

	return utf8_encode($icy);
}

function nice_icy($icy)
{		
	//
	// Check for .mp3 at end.
	//
	
	if (substr($icy,-4) == ".MP3") $icy = substr($icy,0,-4);
	if (substr($icy,-4) == ".mp3") $icy = substr($icy,0,-4);
	if (substr($icy,-3) ==  "mp3") $icy = substr($icy,0,-3);
	if (substr($icy,-3) ==  "...") $icy = substr($icy,0,-3);

	//
	// Check for addition in square brackets.
	//
	
	if (substr(trim($icy),-1) == "]")
	{
		$icy = str_replace("[","(",$icy);
		$icy = str_replace("]",")",$icy);
	}
	
	//
	// Check for semicolon tags.
	//
	
	$parts = explode(";",$icy);
	
	if (count($parts) == 4)
	{
		$icy = $parts[ 1 ] . " - " . $parts[ 2 ];
	}
	
	//
	// Check for single chars > 127 => ISO-LATIN.
	//
	
	$tst = " " . $icy . " ";
	$len = strlen($tst) - 1;
	$iso = false;
	
	for ($inx = 1; $inx < $len; $inx++)
	{
		if ((ord($tst[ $inx - 1 ])  < 128) &&
			(ord($tst[ $inx + 0 ]) >= 128) && 
			(ord($tst[ $inx + 1 ])  < 128))
		{
			$iso = true;
			break;
		}
	}
	
	if ($iso) $icy = utf8_encode($icy);
	
	//
	// Common shit.
	//
	
	$icy = str_replace("_"," ",$icy);
	
	$icy = str_replace("( ","(",$icy);
	$icy = str_replace(" )",")",$icy);
	$icy = str_replace("("," (",$icy);
	
	$icy = str_replace("\\'","'",$icy);
	$icy = str_replace(" / "," - ",$icy);
	$icy = str_replace(" -- "," - ",$icy);
	$icy = str_replace(", - "," - ",$icy);
	$icy = str_replace(" -=- "," - ",$icy);
	
	$icy = str_replace("    "," ",$icy);
	$icy = str_replace("   "," ",$icy);
	$icy = str_replace("  "," ",$icy);
	
	$icy = trim($icy);
	
	//
	// Sie hören bla bla.
	//
	
	if (strstr($icy,"Sie hören \"") !== false)
	{
		$icy = substr($icy,12);
		if (substr($icy,-1) == "\"") $icy = substr($icy,0,-1);
		$icy = str_replace("\" mit \""," - ",$icy);
	}
		
	if (substr($icy,-10) == " auf WDR 2")
	{
		$icy = substr($icy,0,-10);
	}
	
	if (strstr($icy," auf WDR 2 - \"") !== false)
	{
		$icy = str_replace(" auf WDR 2 - \""," - ",$icy);
		if (substr($icy,-1) == "\"") $icy = substr($icy,0,-1);
	}
	
	if (strstr($icy," - Immer Ihre Musik: \"") !== false)
	{
		$icy = str_replace(" - Immer Ihre Musik: \""," - ",$icy);
		if (substr($icy,-1) == "\"") $icy = substr($icy,0,-1);
	}
 
	//
	// No hyphen but " mit ".
	//
	
	if ((strstr($icy, " - " ) === false) && 
		(strstr($icy," mit ") !== false))
	{
		$icy = str_replace(" mit "," - ",$icy);
	}
	
	//
	// No hyphen but " von ".
	//
	
	if ((strstr($icy, " - " ) === false) && 
		(strstr($icy," von ") !== false))
	{
		$parts = explode(" von ",$icy);
		$icy = trim($parts[ 1 ] . " - " . $parts[ 0 ]);
	}
	
	if ((strstr($icy, " - " ) === false) && 
		(strstr($icy,"|") !== false))
	{
		$parts = explode("|",$icy);
		$icy = trim($parts[ 1 ] . " - " . $parts[ 0 ]);
	}
	
	return $icy;
}

function case_icy($icy)
{
	//
	// Check all upper case.
	//
	
	if (strtoupper($icy) == $icy)
	{
		$icy = nice_fup($icy);
	}
	else
	{
		//
		// Check for artist xor title all upper case.
		//
		
		if (strstr($icy," - ") !== false)
		{
			$parts = explode(" - ",$icy);
			
			if (strtoupper($parts[ 0 ]) == $parts[ 0 ])
			{
				$parts[ 0 ] = nice_fup($parts[ 0 ]);

				if ($parts[ 0 ] == "Lmfao" ) $parts[ 0 ] = "LMFAO";
				if ($parts[ 0 ] == "Zz Top") $parts[ 0 ] = "ZZ Top";
				
				$icy = implode(" - ",$parts);
			}
			
			if (strtoupper($parts[ 1 ]) == $parts[ 1 ])
			{
				$parts[ 1 ] = nice_fup($parts[ 1 ]);

				$icy = implode(" - ",$parts);
			}
		}
	}
	
	//
	// Common abbreviations.
	//
	
	$icy = str_replace("Ac/Dc","AC/DC",$icy);
	$icy = str_replace("Ac-Dc","AC/DC",$icy);
	
	$icy = str_replace("&amp;","&",$icy);
	$icy = str_replace(" vs."," vs. ",$icy);
	$icy = str_replace(" ft."," feat. ",$icy);
	$icy = str_replace(" Ft."," feat. ",$icy);
	$icy = str_replace(" Feat."," feat. ",$icy);
	$icy = str_replace("  "," ",$icy);
	
	return $icy;
}

function get_channels($what)
{
	$channels = Array();
	
	$dd = opendir("./$what");
	
	while (($file = readdir($dd)) !== false)
	{
		if ($file == ".") continue;
		if ($file == "..") continue;
		
		array_push($channels,$file);
	}
	
	closedir($dd);
	
	return $channels;
}

function get_channel_config($channel)
{
	$jsonfile = "./channels/$channel/$channel.json";
	$jsoncont = file_get_contents($jsonfile);
	$json = json_decdat($jsoncont);
	
	return $json;
}

function get_icy_title($channel,$setup)
{
	if (! isset($setup[ "broadcast" ])) return null;
	if (! isset($setup[ "broadcast" ][ "streamUrls" ])) return null;
	if (! isset($setup[ "broadcast" ][ "streamUrls" ][ 0 ])) return null;
	
	$streamconf = $setup[ "broadcast" ][ "streamUrls" ][ 0 ];
	
	if (! isset($streamconf[ "streamUrl" ])) return null;
	
	$streamurl = $streamconf[ "streamUrl" ];
	
	$urlparts = parse_url($streamurl);
	
	$host = $urlparts[ "host" ];
	$port = isset($urlparts[ "port" ]) ? $urlparts[ "port" ] : 80;
	$path = isset($urlparts[ "path" ]) ? $urlparts[ "path" ] : "/";
	
	if (isset($urlparts[ "query"    ])) $path .= "?" .$urlparts[ "query"    ];
	if (isset($urlparts[ "fragment" ])) $path .= "?" .$urlparts[ "fragment" ];
	
	$fd = @fsockopen($host,$port,$errno,$errstr,3.0);
	
	if ($fd == null) 
	{
		echo "--------------------> timeout $streamurl\n";
		
		return null;
	}
	
	$header = "GET $path HTTP/1.1\r\nHost:$host\r\nIcy-Metadata:1\r\n\r\n";
	
	fwrite($fd,$header);
	fflush($fd);
	
	fclose($fd);
	return "Tubu";

	$chunksize = 0;

	while (! feof($fd))
	{
		$line = fgets($fd);
		if (! strlen(trim($line))) break;
		//echo "$line";
		if (substr($line,0,12) == "icy-metaint:")
		{	
			$chunksize = (int) substr($line,12);
		}
	}

	//echo "header fettig $chunksize\n";
	$icy = null;
	
	if ($chunksize)
	{
		while (! feof($fd))
		{
			$mp3 = "";

			while ((strlen($mp3) < $chunksize) && ! feof($fd))
			{
				$mp3 .= fread($fd,$chunksize - strlen($mp3));
			}

			if (strlen($mp3) != $chunksize) break;

			$len = fread($fd,1);
			$taglen = 16 * ord($len);
			if ($taglen == 0) return null;
			$icyline = fread($fd,$taglen);
			
			if (! preg_match_all("|StreamTitle='(.*?)';|",$icyline,$icyres)) break;
			
			$icy = $icyres[ 1 ][ 0 ];

			break;
		}
	}
	
	fclose($fd);
	
	return $icy;
}

function open_channel(&$havechannels,&$openchannels,&$deadchannels)
{
	if (count($havechannels) == 0) $havechannels = get_channels("channels");
	if (count($havechannels) == 0) return;
	
	$channel = array_pop($havechannels);
	if (isset($deadchannels[ $channel ])) return;
	if (isset($GLOBALS[ "ignore" ][ $channel ])) return;
	
	$setup = get_channel_config($channel);
	if ($setup == null) return;
	
	if (! isset($setup[ "broadcast" ])) return;
	if (! isset($setup[ "broadcast" ][ "streamUrls" ])) return;
	if (! isset($setup[ "broadcast" ][ "streamUrls" ][ 0 ])) return;
	
	$streamconf = $setup[ "broadcast" ][ "streamUrls" ][ 0 ];
	
	if (! isset($streamconf[ "streamUrl" ])) return;
	$streamurl = $streamconf[ "streamUrl" ];
	
	$oc = Array();
	$oc[ "channel" ] = $channel;
	$oc[ "setup"   ] = $setup;
	$oc[ "url"     ] = $streamurl;
	$oc[ "head"    ] = false;
	$oc[ "headers" ] = Array();
	$oc[ "start"   ] = time();
	
	array_push($oc[ "headers" ],$streamurl);
	array_push($openchannels,$oc);	
}

function process_channel(&$openchannels,&$deadchannels)
{
	for ($inx = 0; $inx < count($openchannels); $inx++)
	{		
		$channel = $openchannels[ $inx ][ "channel" ];
		$start   = $openchannels[ $inx ][ "start"   ];
		$elapsed = time() - $start;
		
		if ($elapsed > 20)
		{
			if (isset($openchannels[ $inx ][ "fd" ]))
			{
				fclose($openchannels[ $inx ][ "fd" ]);
			}
			
			echo "--------------------> elapsed $channel\n";
	
			array_splice($openchannels,$inx--,1);
			continue;
		}
		
		if (! isset($openchannels[ $inx ][ "fd" ]))
		{
			$streamurl = $openchannels[ $inx ][ "url" ];
			$urlparts  = parse_url($streamurl);
			
			$host = $urlparts[ "host" ];
			$port = isset($urlparts[ "port" ]) ? $urlparts[ "port" ] : 80;
			$path = isset($urlparts[ "path" ]) ? $urlparts[ "path" ] : "/";
			
			if (isset($urlparts[ "query"    ])) $path .= "?" .$urlparts[ "query"    ];
			if (isset($urlparts[ "fragment" ])) $path .= "?" .$urlparts[ "fragment" ];
			
			$fd = @fsockopen($host,$port,$errno,$errstr,2.0);
			
			if ($fd == null) 
			{
				$headers = $openchannels[ $inx ][ "headers" ];
				array_push($headers,"Timeout....");
				$headerfile = "./deadchannels/$channel.txt";
				file_put_contents($headerfile,implode("\n",$headers));

				array_splice($openchannels,$inx--,1);
				$deadchannels[ $channel ] = true;
				echo "--------------------> timeout $streamurl\n";
				continue;
			}
			
			$header = "GET $path HTTP/1.1\r\nIcy-Metadata:1\r\n\r\n";
			
			fwrite($fd,$header);
			fflush($fd);
			
			stream_set_blocking($fd,0);
			
			$openchannels[ $inx ][ "fd" ] = $fd;
						
			//echo "-----> $channel\n";

			continue;
		}
		
		$fd = $openchannels[ $inx ][ "fd" ];
		
		if (feof($fd))
		{
			echo "------------------------> " 
				. $channel
				. " (died)\n";
					
			fclose($fd);
			array_splice($openchannels,$inx--,1);
			$deadchannels[ $channel ] = true;
			
			continue;
		}

		if ($openchannels[ $inx ][ "head" ])
		{
			$chunk  = $openchannels[ $inx ][ "chunk"  ];
			$toread = $openchannels[ $inx ][ "toread" ];
			
			if ($toread > 0)
			{
				$mp3data = fread($fd,$toread);
				if ($mp3data == null) continue;
				$toread -= strlen($mp3data);
				$openchannels[ $inx ][ "toread" ] = $toread;
				$GLOBALS[ "downbytes" ] += strlen($mp3data);
			}
			
			if ($toread == 0)
			{
				/*
				echo "-----> " 
					. $channel
					. " (icy!)\n";
				*/
				
				stream_set_blocking($fd,1);

				$len = fread($fd,1);
				$taglen = 16 * ord($len);
				
				if ($taglen == 0)
				{
					/*
					echo "-----> " 
						. $channel
						. " (icy?)\n";
					*/
					
					fclose($fd);
					array_splice($openchannels,$inx--,1);
					$deadchannels[ $channel ] = true;
					
					continue;
				}
				
				$icyline = fread($fd,$taglen);
				
				if (preg_match_all("|StreamTitle='(.*?)';|",$icyline,$icyres))
				{
					$icy = $icyres[ 1 ][ 0 ];
						
					if (! strlen($icy))
					{
						$deadchannels[ $channel ] = true;
					}
					else
					{
						$icy = nice_icy($icy);
						$icy = fixups_icy($channel,$icy);
						$icy = case_icy($icy);

						if (nuke_icy($channel,$icy)) continue;

						$lasticy = "";
						
						$lasticyfile = "./lasticys/$channel.txt";
						if (file_exists($lasticyfile))
						{
							$lasticy = file_get_contents($lasticyfile);
						}
						
						if ($lasticy == $icy)
						{
							continue;
						}

						file_put_contents($lasticyfile,$icy);

						$total = time() - $start;
						$total = str_pad($total,3," ",STR_PAD_LEFT); 
						
						$line = date("Ymd.His")
							  . " "
							  . str_pad($channel,30," ",STR_PAD_RIGHT) 
							  . " $icy\n";
						
						$playlistfile = "./playlists/$channel.txt";
						file_put_contents($playlistfile,$line,FILE_APPEND);
						
						echo $total . " " . $line;
					}
				}
				
				fclose($fd);
				array_splice($openchannels,$inx--,1);
				
				continue;
			}
		}
		else
		{
			$line = fgets($fd);
			if ($line == null) continue;
			
			if ($line == "\r\n")
			{
				/*
				echo "-----> " 
					. $channel
					. " (head)\n";
				*/
				
				if (! isset($openchannels[ $inx ][ "chunk" ]))
				{
					$headers = $openchannels[ $inx ][ "headers" ];
					$headerfile = "./deadchannels/$channel.txt";
					file_put_contents($headerfile,implode("\n",$headers));
					
					fclose($fd);
					array_splice($openchannels,$inx--,1);
					$deadchannels[ $channel ] = true;
					
					continue;
				}
					
				$openchannels[ $inx ][ "head" ] = true;
				continue;
			}
			
			array_push($openchannels[ $inx ][ "headers" ],trim($line));
			
			if (substr($line,0,9) == "Location:")
			{
				$reloc = trim(substr($line,9));
				fclose($fd);

				unset($openchannels[ $inx ][ "fd" ]);
				
				$openchannels[ $inx ][ "url"  ] = $reloc;
				$openchannels[ $inx ][ "head" ] = false;
			}
			
			if (substr($line,0,12) == "icy-metaint:")
			{	
				$openchannels[ $inx ][ "chunk"  ] = (int) substr($line,12);
				$openchannels[ $inx ][ "toread" ] = (int) substr($line,12);
				
				/*
				echo "-----> " 
					. $channel
					. " ("
					. $openchannels[ $inx ][ "chunk" ] 
					. ")\n";
				*/
			}
		}
	}
}
	
	icy_init();
	
	$havechannels = Array();
	$openchannels = Array();
	$deadchannels = Array();

	$lastdead = get_channels("deadchannels");
	foreach ($lastdead as $channel) $deadchannels[ $channel ] = true;
	
	$downbytes = 0;
	$downstamp = 0;
	
	while (true)
	{
		while (count($openchannels) < 40) open_channel($havechannels,$openchannels,$deadchannels);
		
		if (count($openchannels) > 0) process_channel($openchannels,$deadchannels);

		$downbytes = 0;

		usleep(1);
	}
?>