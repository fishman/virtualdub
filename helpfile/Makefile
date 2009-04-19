OUTDIR = ..\..\out\Helpfile
LINA = ..\..\out\$(BUILD)\Lina
HHC = "C:\Program Files\HTML Help Workshop\hhc.exe"

..\$(BUILD)\VirtualDub.vdhelp: $(SRC)/*.lina $(SRC)\pics\*
	if not exist $(OUTDIR) md $(OUTDIR)
	$(LINA) $(SRC)/root.lina
	cd $(OUTDIR)
	cmd /v:on /c $(HHC) help.hhp ^& set /a x = 1-!errorlevel! ^> nul ^& exit !x!
