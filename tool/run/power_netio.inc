proc power_netio_connect { host user_name password } {
	spawn telnet $host 1234
	set connection_id $spawn_id
	expect -i $connection_id "KSHELL V1.*"
	send -i $connection_id "login $user_name $password\n"
	expect -i $connection_id "250 OK"

	return $connection_id
}
