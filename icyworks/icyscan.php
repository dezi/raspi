<?php

include "json.php";

function icy_configure()
{	
	if (isset($GLOBALS[ "icy_configtime" ]) && 
		((time() - $GLOBALS[ "icy_configtime" ]) < 10))
	{
		return;
	}
	
	$GLOBALS[ "icy_configtime" ] = time();
	
	$icyjson = file_get_contents("./icyscan.json");
	$icyscan = json_decdat($icyjson);
	
	if (! $icyscan)
	{
		echo "--------------------> config fucked...\n";
	}
	
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
			
			//echo "====> fixups str_replace($from,$toto) => $icy\n";

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
			
			//echo "====> fixups preg_replace($pattern) => $icy\n";

			$icy = trim(preg_replace($pattern,$toto,$icy));
		}
	}
	
	return $icy;
}

function query_icy($channel,$icy)
{
	$parts = explode(" - ",$icy);
	if (count($parts) != 2) return "?";
	
	if (strstr($icy,"|")) return "?";
	if (strstr($icy,":")) return "?";
	if (strstr($icy,"/")) return "?";
	
	if ($parts[ 0 ] != mb_convert_case($parts[ 0 ],MB_CASE_TITLE,"UTF-8")) return "?";
	if ($parts[ 1 ] != mb_convert_case($parts[ 1 ],MB_CASE_TITLE,"UTF-8")) return "?";
	
	//
	// Looks reasonable, give it a try.
	//
	
	$icyname = "./library/$icy.json";
	if (file_exists($icyname)) return '*';
	
	$url = "http://api.discogs.com/database/search?q=" . urlencode($icy);
	$discogsjson = file_get_contents($url,false,$GLOBALS[ "context" ]);

	if ($discogsjson === false) return "!";
	$discogs = json_decdat($discogsjson);
	if (! $discogs) return "#";
	
	if (! isset($discogs[ "results" ])) return '-';
	
	$icyjson = json_encdat($discogs[ "results" ]);
	
	file_put_contents($icyname,$icyjson);
	
	return "+";
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
			
			if (strstr(strtolower($icy),strtolower($pattern)) !== false) 
			{
				//echo "====> nuke strstr($pattern) => $icy\n";

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
				//echo "====> nuke preg_match($pattern) => $icy\n";

				return true;
			}
		}
	}

	return false;
}

function nice_fup($icy)
{
	//$icy = mb_convert_case($icy,MB_CASE_LOWER,"UTF-8");
	$icy = mb_convert_case($icy,MB_CASE_TITLE,"UTF-8");
	
	/*
	$tst = " " . $icy;
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
	*/
	
	return $icy;
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
	if (substr($icy,-1) ==    "#") $icy = substr($icy,0,-1);

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
	$icy = str_replace("´","'",$icy);
	$icy = str_replace("`","'",$icy);
	
	$icy = str_replace("( ","(",$icy);
	$icy = str_replace(" )",")",$icy);
	$icy = str_replace("("," (",$icy);
	
	$icy = str_replace("\\'","'",$icy);
	$icy = str_replace("\""," ",$icy);
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
	
	if (substr($icy,-1) == "-") $icy = trim(substr($icy,0,-1));
	if (substr($icy,-1) == "*") $icy = trim(substr($icy,0,-1));
	
	return $icy;
}

function reverse_icy($channel,$icy)
{
	if (isset($GLOBALS[ "reverse" ][ $channel ]))
	{
		$parts = explode(" - ",$icy);
		
		if (count($parts) == 2)
		{
			$temps = $parts[ 0 ];
			$parts[ 0 ] = $parts[ 1 ];
			$parts[ 1 ] = $temps;
			$icy = implode(" - ",$parts);
		}
	}
	
	return $icy;
}

function case_icy($icy)
{
	//
	// Check all upper case.
	//
	
	/*
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

				$icy = implode(" - ",$parts);
			}
			
			if (strtoupper($parts[ 1 ]) == $parts[ 1 ])
			{
				$parts[ 1 ] = nice_fup($parts[ 1 ]);

				$icy = implode(" - ",$parts);
			}
		}
	}
	*/
	
	$icy = nice_fup($icy);

	//
	// Common abbreviations.
	//
	
	$icy = str_replace("Ac Dc","Ac-Dc",$icy);
	$icy = str_replace("Ac/Dc","Ac-Dc",$icy);
	$icy = str_replace("Ac/dc","Ac-Dc",$icy);

	$icy = str_replace("&acute;","'",$icy);
	$icy = str_replace("&amp;","&",$icy);
	$icy = str_replace("&#039;","'",$icy);
	
	$icy = str_replace(" vs."," Vs. ",$icy);
	$icy = str_replace(" Vs."," Vs. ",$icy);
	$icy = str_replace(" ft."," Feat. ",$icy);
	$icy = str_replace(" Ft."," Feat. ",$icy);
	$icy = str_replace(" Feat."," Feat. ",$icy);
	
	$icy = trim(str_replace("(Radio)"," ",$icy));
	$icy = trim(str_replace("(Radio Edit)"," ",$icy));
	$icy = trim(str_replace("(Radio Version)"," ",$icy));
	
	$icy = str_replace("   "," ",$icy);
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
			
			if (! isset($urlparts[ "host" ]))
			{
				$headers = $openchannels[ $inx ][ "headers" ];
				array_push($headers,"Badurl....");
				$headerfile = "./deadchannels/$channel.txt";
				file_put_contents($headerfile,implode("\n",$headers) . "\n");
				$deadchannels[ $channel ] = true;

				array_splice($openchannels,$inx--,1);
				echo "--------------------> invalid $streamurl\n";
				continue;
			}
			
			$host = $urlparts[ "host" ];
			$port = isset($urlparts[ "port" ]) ? $urlparts[ "port" ] : 80;
			$path = isset($urlparts[ "path" ]) ? $urlparts[ "path" ] : "/";
			
			if (isset($urlparts[ "query"    ])) $path .= "?" .$urlparts[ "query"    ];
			if (isset($urlparts[ "fragment" ])) $path .= "?" .$urlparts[ "fragment" ];
			
			$fd = @fsockopen($host,$port,$errno,$errstr,4.0);
			
			if ($fd == null) 
			{
				array_splice($openchannels,$inx--,1);
				echo "--------------------> timeout $streamurl\n";
				continue;
			}
			
			$header = "GET $path HTTP/1.1\r\nHost: $host\r\nIcy-Metadata: 1\r\n\r\n";
			
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
			fclose($fd);
			array_splice($openchannels,$inx--,1);

			echo "------------------------> " 
				. $channel
				. " (died)\n";
					
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
					
					continue;
				}
				
				$icyline = fread($fd,$taglen);
				
				if (preg_match_all("|StreamTitle='(.*?)';|",$icyline,$icyres))
				{
					$icy = $icyres[ 1 ][ 0 ];
						
					if (! strlen($icy))
					{

					}
					else
					{
						$icy = nice_icy($icy);
						$icy = fixups_icy($channel,$icy);
						$icy = case_icy($icy);
						$icy = reverse_icy($channel,$icy);

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

						$query = query_icy($channel,$icy);
						$total = time() - $start;
						$total = str_pad($total,2," ",STR_PAD_LEFT); 
						$opens = count($openchannels); 
						$opens = str_pad($opens,2," ",STR_PAD_LEFT); 
						$kbits = $GLOBALS[ "downbytes" ] * 10;
						$kbits = $kbits / (time() - $GLOBALS[ "downstamp" ]);
						$kbits = (int) ($kbits / 1024);
						$kbits = str_pad($kbits,5," ",STR_PAD_LEFT); 

						$line = date("Ymd.His")
							  . " "
							  . str_pad($channel,30," ",STR_PAD_RIGHT) 
							  . " $icy\n";
						
						$playlistfile = "./playlists/$channel.txt";
						file_put_contents($playlistfile,$line,FILE_APPEND);
						
						$line = date("Ymd.His")
							  . " "
							  . str_pad($channel,30," ",STR_PAD_RIGHT) 
							  . " $query$icy\n";
						
						
						echo $total . " " . $opens . " " . $kbits . " kbit/s " . $line;
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
					array_push($headers,"Nochunk....");
					$headerfile = "./deadchannels/$channel.txt";
					file_put_contents($headerfile,implode("\n",$headers) . "\n");
					$deadchannels[ $channel ] = true;
					
					fclose($fd);
					array_splice($openchannels,$inx--,1);
					
					continue;
				}
					
				$openchannels[ $inx ][ "head" ] = true;
				continue;
			}
			
			array_push($openchannels[ $inx ][ "headers" ],trim($line));
			
			if (substr($line,1,8) == "ocation:")
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
	
	$havechannels = Array();
	$openchannels = Array();
	$deadchannels = Array();

	$lastdead = get_channels("deadchannels");
	foreach ($lastdead as $channel) $deadchannels[ $channel ] = true;
	
	$options = array
	(
  		'http' => array
  		(
    		'method'=>"GET",
    		'header'=>"User-Agent: KappaRadioPlaylistTracker/0.1 (dezi@kappa-mm.de) +http://www.kappa-mm.de/\r\n"
  		)
	);

	$context = stream_context_create($options);
	
	$downbytes = 0;
	$downstamp = time() - 1;
	
	while (true)
	{
		icy_configure();
		
		while (count($openchannels) < 40) open_channel($havechannels,$openchannels,$deadchannels);
		
		if (count($openchannels) > 0) process_channel($openchannels,$deadchannels);

		if (($act = (time() - $downstamp)) > 10)
		{
			$downbytes = (int) ($downbytes / 2);
			$downstamp = time() - ($act / 2);
		}
		
		usleep(1);
	}
?>
