#!/bin/sh -e
# Validate test XML data against the DTDs.

XMLLINT_ARGS="--noout --dtdvalid"

applications="Gallery.application Mailer.application"

for application in $applications
do
	${XMLLINT} $XMLLINT_ARGS ${DTDDIR}accounts-application.dtd ${TESTDATADIR}$application || exit 1
done

${XMLLINT} $XMLLINT_ARGS ${DTDDIR}accounts-provider.dtd ${TESTDATADIR}MyProvider.provider

services="MyService.service MyService2.service OtherService.service"

for service in $services
do
	${XMLLINT} $XMLLINT_ARGS ${DTDDIR}accounts-service.dtd ${TESTDATADIR}$service || exit 1
done

${XMLLINT} $XMLLINT_ARGS ${DTDDIR}accounts-service-type.dtd ${TESTDATADIR}e-mail.service-type
