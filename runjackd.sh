killall -9 jackd 2>/dev/null

jackd -d opensles -p 512 -r 44100 &

sleep 2
jack_lsp
