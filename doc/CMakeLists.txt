# Copyright (c) 2019-2021 The Fittexxcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

configure_file(Doxyfile.cmake.in Doxyfile ESCAPE_QUOTES)

option(DOC_ONLINE "Adapt Markdown/HTML documentation for online publication" OFF)
configure_file(
	../cmake/utils/gen-doc-md.sh.in
	gen-doc-md.sh
	@ONLY
)
add_custom_target(doc-md
	COMMENT "Building Markdown documentation..."
	DEPENDS fittexxcoind
	DEPENDS fittexxcoin-qt
	DEPENDS fittexxcoin-cli
	DEPENDS fittexxcoin-tx
	DEPENDS fittexxcoin-seeder
	DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/gen-doc-md.sh"
	COMMAND "${CMAKE_CURRENT_BINARY_DIR}/gen-doc-md.sh"
)
add_custom_target(doc-html
	COMMENT "Building HTML documentation..."
	DEPENDS doc-md
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	COMMAND mkdocs build
)

option(ENABLE_MAN "Enable manpages" ON)
if(ENABLE_MAN AND BUILD_FITTEXXCOIN_DAEMON)

	# help2man is required by the `gen-doc-man.sh` script.
	find_program(HELP2MAN "help2man")
	if(NOT HELP2MAN)
		message(
			FATAL_ERROR
			"Failed to find the program 'help2man' required for building manpages. "
			"Please make sure that it is installed and reachable through the system PATH. "
			"To disable manpages, add cmake build option '-DENABLE_MAN=OFF'."
		)
	endif()

	configure_file(
		../cmake/utils/gen-doc-man-footer.sh.in
		man/gen-doc-man-footer.sh
		@ONLY
	)
	add_custom_command(
		OUTPUT man/footer.h2m
		DEPENDS fittexxcoind
		DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man-footer.sh"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
		COMMAND ./gen-doc-man-footer.sh
	)
	configure_file(
		../cmake/utils/gen-doc-man.sh.in
		man/gen-doc-man.sh
		@ONLY
	)
	include(GNUInstallDirs)
	set(MAN_DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
	add_custom_command(
		OUTPUT man/fittexxcoind.1
		DEPENDS fittexxcoind
		DEPENDS man/footer.h2m
		DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
		COMMAND ./gen-doc-man.sh fittexxcoind
	)
	add_custom_target(doc-man-fittexxcoind
		ALL
		DEPENDS man/fittexxcoind.1
	)
	install(
		FILES "${CMAKE_CURRENT_BINARY_DIR}/man/fittexxcoind.1"
		DESTINATION "${MAN_DESTINATION}"
		COMPONENT fittexxcoind
	)
	if(BUILD_FITTEXXCOIN_QT)
		add_custom_command(
			OUTPUT man/fittexxcoin-qt.1
			DEPENDS fittexxcoin-qt
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh qt/fittexxcoin-qt
		)
		add_custom_target(doc-man-fittexxcoin-qt
			ALL
			DEPENDS man/fittexxcoin-qt.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/fittexxcoin-qt.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT fittexxcoin-qt
		)
	endif()
	if(BUILD_FITTEXXCOIN_CLI)
		add_custom_command(
			OUTPUT man/fittexxcoin-cli.1
			DEPENDS fittexxcoin-cli
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh fittexxcoin-cli
		)
		add_custom_target(doc-man-fittexxcoin-cli
			ALL
			DEPENDS man/fittexxcoin-cli.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/fittexxcoin-cli.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT fittexxcoind
		)
	endif()
	if(BUILD_FITTEXXCOIN_TX)
		add_custom_command(
			OUTPUT man/fittexxcoin-tx.1
			DEPENDS fittexxcoin-tx
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh fittexxcoin-tx
		)
		add_custom_target(doc-man-fittexxcoin-tx
			ALL
			DEPENDS man/fittexxcoin-tx.1
		)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/fittexxcoin-tx.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT fittexxcoind
		)
	endif()
	if(BUILD_FITTEXXCOIN_SEEDER)
		add_custom_command(
			OUTPUT man/fittexxcoin-seeder.1
			DEPENDS fittexxcoin-seeder
			DEPENDS man/footer.h2m
			DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/man/gen-doc-man.sh"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/man"
			COMMAND ./gen-doc-man.sh seeder/fittexxcoin-seeder
		)
		add_custom_target(doc-man-fittexxcoin-seeder
			ALL
			DEPENDS man/fittexxcoin-seeder.1)
		install(
			FILES "${CMAKE_CURRENT_BINARY_DIR}/man/fittexxcoin-seeder.1"
			DESTINATION "${MAN_DESTINATION}"
			COMPONENT fittexxcoin-seeder
		)
	endif()
	add_custom_target(doc-man
		DEPENDS doc-man-fittexxcoind
		DEPENDS doc-man-fittexxcoin-qt
		DEPENDS doc-man-fittexxcoin-cli
		DEPENDS doc-man-fittexxcoin-tx
		DEPENDS doc-man-fittexxcoin-seeder
	)
endif()

