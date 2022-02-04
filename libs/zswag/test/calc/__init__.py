import zserio
from os.path import dirname, abspath

working_dir = dirname(abspath(__file__))
zserio.generate(
    zs_dir=working_dir,
    main_zs_file="calculator.zs",
    gen_dir=working_dir,
    extra_args=["-withTypeInfoCode"])
