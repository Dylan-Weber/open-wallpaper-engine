import rstd;
import rstd.test;
import rstd.log;

int main(int argc, char** argv) {
    rstd::env::args_init(argc, argv);
    // an error a script throws is logged; without a logger that aborted the whole test binary
    static rstd::log::EnvLogger logger;
    rstd::log::set_logger(logger);
    rstd::log::set_max_level(logger.filter());
    return rstd::test::run_registered().to_primitive();
}
