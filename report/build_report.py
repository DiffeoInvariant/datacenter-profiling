from pytex import TestReport, Section, Subsection, Image, Figure, CodeSnippet, UsePackage, TextLines


report = TestReport('report.tex')
report.set_title('CSCI 5253 Final Project Report',author='Emily Jakobs',use_date=True)
report.add_required_packages((UsePackage('url'),UsePackage('listings'),UsePackage('fancyvrb')))
report.append(Section.from_file('overview.tex','Overview And Goals'))
report.append(Section.from_file('components.tex','Description of Components'))
report.append(Section.from_file('capabilities.tex','Capabilities and Limitations'))
header_text = TextLines.from_file('/home/diffeoinvariant/datacenter-profiling/programs/slepc/petsc_webserver.h')
report.append(Section('Appendix A',['This code --- available at \\url{https://github.com/DiffeoInvariant/datacenter-profiling} --- defines the public API of the mini-library that powers the driver program, itself available along with the implementation of this header and the Python webserver code in that git repository.'],starred=True))
report.add_code_snippet(header_text,language='C',xleftmargin='-2cm',xrightmargin='-2cm')
output = report.compile().stdout
print(f"Output is \n{output}")
