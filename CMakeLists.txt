kde4_header()

add_subdirectory( vnc ) 
add_subdirectory( rdp ) 

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(krdc_SRCS 
   main.cpp 
   krdc.cpp 
   vidmode.cpp 
   kfullscreenpanel.cpp 
   kservicelocator.cpp 
   keycapturedialog.cpp 
   kremoteview.cpp 
   smartptr.cpp 
   hostpreferences.cpp 
   preferencesdialog.cpp 
   maindialogwidget.cpp 
   maindialog.cpp )

kde4_automoc(${krdc_SRCS})

kde4_add_ui3_files(krdc_SRCS hostprofiles.ui3 maindialogbase.ui3 keycapturewidget.ui3 )

kde4_add_executable(krdc ${krdc_SRCS})

target_link_libraries(krdc  ${KDE4_KDEUI_LIBS} vnc rdp kdnssd kwalletclient )

install_targets(/bin krdc )


########### install files ###############

install_files( ${XDG_APPS_DIR} FILES krdc.desktop )
install_files( ${DATA_INSTALL_DIR}/konqueror/servicemenus FILES smb2rdc.desktop )
install_files( ${DATA_INSTALL_DIR}/krdc/pics FILES pointcursor.png pointcursormask.png pinup.png pindown.png    iconify.png close.png )
install_files( ${SERVICES_INSTALL_DIR} FILES vnc.protocol rdp.protocol )
install_files( ${DATA_INSTALL_DIR}/zeroconf FILES _rfb._tcp )

kde4_install_icons( ${ICON_INSTALL_DIR}  hicolor )

kde4_footer()



#original Makefile.am contents follow:

#KDE_CXXFLAGS = $(USE_THREADS)
#
#SUBDIRS = vnc rdp
#
#METASOURCES = AUTO
#
#bin_PROGRAMS = krdc
#krdc_SOURCES = main.cpp krdc.cpp vidmode.cpp kfullscreenpanel.cpp \
#  hostprofiles.ui maindialogbase.ui keycapturewidget.ui kservicelocator.cpp \
#  keycapturedialog.cpp kremoteview.cpp smartptr.cpp hostpreferences.cpp \
#  preferencesdialog.cpp maindialogwidget.cpp maindialog.cpp
#
#noinst_HEADERS = main.h krdc.h vidmode.h kfullscreenpanel.h events.h \
#  kservicelocator.h preferencesdialog.h \
#  keycapturedialog.h kremoteview.h smartptr.h hostpreferences.h
#
#EXTRA_DIST = README TODO
#
#appdatadir = $(kde_datadir)/krdc/pics
#appdata_DATA =  pointcursor.png pointcursormask.png pinup.png pindown.png \
#  iconify.png close.png
#
#dnssddatadir = $(kde_datadir)/zeroconf
#dnssddata_DATA = _rfb._tcp
#
#krdc_LDADD   = vnc/libvnc.la rdp/librdp.la $(LIB_KDEUI) $(LIBXF86VIDMODE) $(LIB_SLP) $(LIB_KDNSSD) $(X_LDFLAGS) $(LIB_X11) -lkwalletclient
#krdc_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#
#xdg_apps_DATA = krdc.desktop
#
#servicedir = $(kde_datadir)/konqueror/servicemenus
#service_DATA = smb2rdc.desktop
#
#kde_services_DATA = vnc.protocol rdp.protocol
#
#KDE_ICON = krdc
#
#INCLUDES= $(all_includes)
#
#messages: rc.cpp
#	$(EXTRACTRC) *.ui */*.ui >> rc.cpp
#	$(XGETTEXT) *.cpp */*.cpp *.h -o $(podir)/krdc.pot