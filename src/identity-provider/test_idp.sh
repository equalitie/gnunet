#!/bin/bash
#trap "gnunet-arm -e -c test_idp_lookup.conf" SIGINT

LOCATION=$(which gnunet-config)
if [ -z $LOCATION ]
then
  LOCATION="gnunet-config"
fi
$LOCATION --version 1> /dev/null
if test $? != 0
then
	echo "GNUnet command line tools cannot be found, check environmental variables PATH and GNUNET_PREFIX"
	exit 77
fi

rm -rf `gnunet-config -c test_idp.conf -s PATHS -o GNUNET_HOME -f`

#  (1) PKEY1.user -> PKEY2.resu.user
#  (2) PKEY2.resu -> PKEY3
#  (3) PKEY3.user -> PKEY4


which timeout &> /dev/null && DO_TIMEOUT="timeout 30"

TEST_ATTR="test"
gnunet-arm -s -c test_idp.conf
gnunet-identity -C testego -c test_idp.conf
valgrind gnunet-idp -e testego -a email -V john@doe.gnu -c test_idp.conf
gnunet-idp -e testego -a name -V John -c test_idp.conf
gnunet-idp -e testego -D -c test_idp.conf
gnunet-arm -e -c test_idp.conf
