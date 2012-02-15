@ touch the resource file, so that ucc will be triggered
copy /b *.qrc +,,

@ generate the date file, which is included in the resource file
date /T > CompileDate.txt